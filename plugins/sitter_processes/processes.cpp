// Snap Websites Server -- watchdog processes
// Copyright (c) 2013-2019  Made to Order Software Corp.  All Rights Reserved
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


// self
//
#include    "processes.h"

#include    "names.h"


// sitter
//
#include    <sitter/exception.h>


// cppprocess
//
#include    <cppprocess/process.h>
#include    <cppprocess/io_capture_pipe.h>


// advgetopt
//
#include    <advgetopt/conf_file.h>


// snaplogger
//
#include    <snaplogger/message.h>


// snapdev
//
#include    <snapdev/enumerate.h>
#include    <snapdev/file_contents.h>
#include    <snapdev/glob_to_list.h>
#include    <snapdev/not_reached.h>
#include    <snapdev/not_used.h>
#include    <snapdev/trim_string.h>


// serverplugins
//
#include    <serverplugins/collection.h>


// C++
//
#include    <regex>


// last include
//
#include    <snapdev/poison.h>



namespace sitter
{
namespace processes
{

SERVERPLUGINS_START(processes, 1, 0)
    , ::serverplugins::description(
            "Check whether a set of processes are running.")
    , ::serverplugins::dependency("server")
    , ::serverplugins::help_uri("https://snapwebsites.org/help")
    , ::serverplugins::categorization_tag("process")
SERVERPLUGINS_END(processes)



namespace
{


char const * g_server_configuration_filename = "/etc/snapwebsites/snapserver.conf";

char const * g_configuration_apache2_maintenance = "/etc/apache2/snap-conf/snap-apache2-maintenance.conf";


/** \brief Check whether a service is enabled or not.
 *
 * The Snap! Watchdog does view a missing process as normal if the
 * corresponding service is marked as disabled. This function tells
 * us whether the service is considered up and running or not.
 *
 * When the XML file includes the \<service> tag, it calls this
 * function. If the function returns false, then no further test
 * is done and the process entry is ignored.
 *
 * \note
 * This means a process that's turned off for maintenance does not
 * generate errors for being turned off during that time OR AFTER
 * IF YOU FORGET TO TURN IT BACK ON. A later version may want to
 * have a way to know whether the process is expected to be on and
 * if so still generate an error after X hours of being down...
 * (or once the system is back up, i.e., it's not in maintenance
 * mode anymore.) However, at this point we do not know which
 * snapbackend are expected to be running.
 *
 * \param[in] service_name  The name of the service, as systemd understands
 *            it, to check on.
 *
 * \return true if the service is marked as enabled.
 */
bool is_service_enabled(std::string const & service_name)
{
    // here I use the `show` command instead of the `is-enabled` to avoid
    // errors whenever the service is not even installed, which can happen
    // (i.e. clamav-freshclam is generally only installed on one system in
    // the entire cluster)
    //
    cppprocess::process p("query service status");
    p.set_command("systemctl");
    p.add_argument("show");
    p.add_argument("-p");
    p.add_argument("UnitFileState");
    p.add_argument("--value"); // available since systemd 230, so since Ubuntu 18.04
    p.add_argument(service_name);
    cppprocess::io_capture_pipe::pointer_t out(std::make_shared<cppprocess::io_capture_pipe>());
    p.set_output_io(out);
    int r(p.start());
    if(r == 0)
    {
        r = p.wait();
    }
    std::string const output(out->get_trimmed_output());
    SNAP_LOG_INFO
        << "\"show -p UnitFileState\" query output ("
        << r
        << "): "
        << output
        << SNAP_LOG_SEND;


    // we cannot use 'r' since it is 0 if the command works whether or not
    // the corresponding unit even exist on the system
    //
    // so instead we just have to test the output and it must be exactly
    // equal to the following
    //
    // (other possible values are static, disabled, and an empty value for
    // non-existant units.)
    //
    return out->get_output() == "enabled";


//    snap::process p("query service status");
//    p.set_mode(snap::process::mode_t::PROCESS_MODE_OUTPUT);
//    p.set_command("systemctl");
//    p.add_argument("is-enabled");
//    p.add_argument(service_name);
//    int const r(p.run());
//    QString const output(p.get_output(true).trimmed());
//    SNAP_LOG_INFO
//        << "\"is-enabled\" query output ("
//        << r
//        << "): "
//        << output
//        << SNAP_LOG_SEND;
//
//    // there is a special case with static services: the is-enabled returns
//    // true (r == 0) even when they are not enabled
//    //
//    if(output == "static")
//    {
//        return false;
//    }
//
//    return r == 0;
}


/** \brief Check whether a service is active or not.
 *
 * The Snap! Watchdog checks whether a service is considered active too.
 * A service may be marked as enabled but it may not be active.
 *
 * \param[in] service_name  The name of the service, as systemd understands
 *            it, to check on.
 *
 * \return true if the service is marked as active.
 */
bool is_service_active(std::string const & service_name)
{
    cppprocess::process p("query service status");
    p.set_command("systemctl");
    p.add_argument("is-active");
    p.add_argument(service_name);
    cppprocess::io_capture_pipe::pointer_t out(std::make_shared<cppprocess::io_capture_pipe>());
    p.set_output_io(out);
    int r(p.start());
    if(r == 0)
    {
        r = p.wait();
    }
    SNAP_LOG_INFO
        << "\"is-active\" query output ("
        << r
        << "): "
        << out->get_trimmed_output()
        << SNAP_LOG_SEND;
    return r == 0;
}


/** \brief Check whether the system is in maintenance mode.
 *
 * This function checks whether the standard maintenance mode is currently
 * turn on or not. This is done by checking the maintenance Apache
 * configuration file and see whether the lines between ##MAINTENANCE-START##
 * and ##MAINTENANCE-END## are commented out or not.
 *
 * \return true if the maintenance mode is ON.
 */
bool is_in_maintenance()
{
    snapdev::file_contents conf(g_configuration_apache2_maintenance);
    if(!conf.exists())
    {
        // the maintenance file doesn't exist, assume the worst, that
        // we are not in maintenance
        //
        return false;
    }

    std::string const contents(conf.contents());
    std::string::size_type const pos(contents.find("##MAINTENANCE-START##"));
    if(pos == std::string::npos)
    {
        // marker not found... consider we are live
        //
        return false;
    }

    char const * s(contents.c_str() + pos + 21);
    while(isspace(*s))
    {
        ++s;
    }
    if(*s == '#')
    {
        // not in maintenance, fields are commented out
        //
        return false;
    }

    std::string::size_type const ra_pos(contents.find("Retry-After"));
    if(ra_pos == std::string::npos)
    {
        // no Retry-After header?!
        //
        return false;
    }

    return true;
}


/** \brief Class used to read the list of processes to check.
 *
 * This class holds the configuration for one process.
 *
 * The class understands the following configuration format:
 *
 * \code
 *     name=<process name>
 *     mandatory=<true | false>
 *     allow_duplicates=<true | false>
 *     command=<command path>
 *     service=<name>
 *     backend=<true | false>
 *     match=<regex>
 * \endcode
 *
 * Create additional configuration files to define additional processes.
 *
 * The `service=...` means that we have a `<project>.service`. That service
 * is then expected to be up and running.
 *
 * The `backend` boolean flag is used to define whether the `service=...`
 * is a backend or not. If not, then the backend status is ignored. If it
 * is, then if backends are currently turned off (inactive) then an inactive
 * state for that service is expected and not viewed as an error.
 */
class sitter_process
{
public:
    typedef std::vector<sitter_process>     vector_t;

                                sitter_process(std::string const & name, bool mandatory, bool allow_duplicates);

    void                        set_mandatory(bool mandatory);
    void                        set_command(std::string const & command);
    void                        set_service(std::string const & service, bool backend);
    void                        set_match(std::string const & match);

    std::string const &         get_name() const;
    bool                        is_mandatory() const;
    bool                        is_backend() const;
    bool                        is_process_expected_to_run();
    bool                        allow_duplicates() const;
    bool                        match(std::string const & name, std::string const & cmdline);

private:
    static advgetopt::string_list_t
                                g_valid_backends;

    std::string                 f_name = std::string();
    std::string                 f_command = std::string();
    std::string                 f_service = std::string();
    std::regex                  f_match = std::regex();
    bool                        f_match_defined = false;
    bool                        f_mandatory = false;
    bool                        f_allow_duplicates = false;
    bool                        f_service_is_enabled = true;
    bool                        f_service_is_active = true;
    bool                        f_service_is_backend = false;
};


// define static variable
//
advgetopt::string_list_t    sitter_process::g_valid_backends;


sitter_process::vector_t    g_processes;


/** \brief Initializes a sitter_process class.
 *
 * This function initializes the sitter_process making it ready to
 * run the match() command.
 *
 * To complete the setup, when available, the set_command() and
 * set_match() functions should be called.
 *
 * \param[in] name  The name of the command, in most cases this is the
 *            same as the terminal command line name.
 * \param[in] mandatory  Whether the process is mandatory (error with a
 *            very high priority if not found.)
 * \param[in] allow_duplicates  Whether this entry can be defined more than
 *            once within various XML files.
 */
sitter_process::sitter_process(std::string const & name, bool mandatory, bool allow_duplicates)
    : f_name(name)
    , f_mandatory(mandatory)
    , f_allow_duplicates(allow_duplicates)
{
}


/** \brief Set whether this process is mandatory or not.
 *
 * Change the mandatory flag.
 *
 * At the moment this is used by the loader to force the mandatory flag
 * when a duplicate is found and the new version is mandatory. In other
 * word, it is a logical or between all the instances of the process
 * found on the system.
 *
 * \param[in] mandatory  Whether this process is mandatory.
 */
void sitter_process::set_mandatory(bool mandatory)
{
    f_mandatory = mandatory;
}


/** \brief Set the name of the expected command.
 *
 * The name of the watchdog process may be different from the exact
 * terminal command name. For example, the cassandra process runs
 * using "java" and not "cassandra". In that case, the command would
 * be set "java".
 *
 * \param[in] command  The name of the command to seek.
 */
void sitter_process::set_command(std::string const & command)
{
    f_command = command;
}


/** \brief Set the name of the service corresponding to this process.
 *
 * When testing whether a process is running, the watchdog can first
 * check whether that process is a service (i.e. when a service name was
 * specified in the XML.) When a process is a known service and the
 * service is disabled, then whether the service is running is none of
 * our concern. However, if enabled and the service is not running,
 * then there is a problem.
 *
 * Note that by default a process is not considered a service. You
 * have to explicitely mark it as such with the \<service> tag.
 * This also allows you to have a name for the service which is
 * different than the name of the executable (i.e. "snapwatchdog"
 * is the service and "snapwatchdogserver" is the executable.)
 *
 * You may reset the service to QString(). In that case, it resets
 * the flags to their defaults and ignores the \p backend parameter.
 *
 * \param[in] service  The name of the service to check.
 * \param[in] backend  Whether the service is a snapbackend.
 */
void sitter_process::set_service(std::string const & service, bool backend)
{
    // we check whether the service is running just once here
    // (otherwise we could end up calling that function once per
    // process!)
    //
    f_service = service;

    if(f_service.empty())
    {
        f_service_is_enabled = true;
        f_service_is_active = true;
        f_service_is_backend = false;
    }
    else
    {
        f_service_is_enabled = is_service_enabled(service);
        f_service_is_active = f_service_is_enabled
                                    ? is_service_active(service)
                                    : false;
        f_service_is_backend = backend;
    }
}


/** \brief Define the match regular expression.
 *
 * If the process has a complex command line definition to be checked,
 * then this regular expression can be used. For example, to check
 * whether Cassandra is running, we search for a Java program which
 * runs the Cassandra system. This is done using a regular expression:
 *
 * \code
 *   <match>java.*org\.apache\.cassandra\.service\.CassandraDaemon</match>
 * \endcode
 *
 * (at the moment, though, we have a specialized Cassandra plugin and
 * thus this is not part of the list of processes in our XML files.)
 *
 * \param[in] match  A valid regular expression.
 */
void sitter_process::set_match(std::string const & match)
{
    f_match_defined = !match.empty();
    f_match = match;
}


/** \brief Get the name of the process.
 *
 * This function returns the name of the process. Note that the
 * terminal command line may be different.
 *
 * \return The name that process was given.
 */
std::string const & sitter_process::get_name() const
{
    return f_name;
}


/** \brief Check whether this process is considered mandatory.
 *
 * This function returns the mandatory flag.
 *
 * By default processes are not considered mandatory. Add the
 * mandatory attribute to the tag to mark a process as mandatory.
 *
 * This flag tells us what priority to use when we generate an
 * error when a process can't be found. 60 when not mandatory
 * and 95 when mandatory.
 *
 * \return true if the mandatory flag is set.
 */
bool sitter_process::is_mandatory() const
{
    return f_mandatory;
}


/** \brief Check whether this process is a backend service.
 *
 * Whenever a process is marked as a service, it can also specifically
 * be marked as a backend service.
 *
 * A backend service is not forcibly expected to be running whenever
 * the system is put in maintenance mode. This flag is used to test
 * that specific status.
 *
 * \return true if the process was marked as a backend service.
 */
bool sitter_process::is_backend() const
{
    return f_service_is_backend;
}


/** \brief Check whether a backend is running or not.
 *
 * This functin is used to determine whether the specified backend service
 * is expected to be running or not.
 *
 * If the main flag (`backend_status`) is set to `disabled`, then the
 * backend service is viewed as disabled and this function returns
 * false.
 *
 * When the `backend_status` is not set to `disabled` the function further
 * checks on the backends list of services and determine whether the named
 * process is defined there. If so, then it is considered `enabled` (i.e.
 * it has to be running since the user asks for it to be running.)
 *
 * \return true if the backend is enabled (expected to run), false otherwise
 */
bool sitter_process::is_process_expected_to_run()
{
    // is this even marked as a service?
    // if not then it has to be running
    //
    // (i.e. services which we do not offer to disable are expected to always
    // be running--except while upgrading or rebooting which we should also
    // look into TODO)
    //
    if(f_service.empty())
    {
        return true;
    }

    // we have two cases:
    //
    // 1. backend services
    //
    // 2. other more general services
    //
    // we do not handle them the same way at all, backends have two flags
    // to check (first block below) and we completely ignore the status
    // of the service
    //
    // as for the more general services they just have their systemd status
    // (i.e. whether they are active or disabled)
    //
    if(is_backend())
    {
        // all the backend get disabled whenever the administrator sets
        // the "backend_status" flag to "disabled", this is global to all
        // the computers of a cluster (at least it is expected to be that way)
        //
        // whatever other status does not matter if this flag is set to
        // disabled then the backed is not expected to be running
        //
        // note: configuration files are cached so the following is rather
        //       fast the second time (i.e. access an std::map<>().)
        //
        advgetopt::conf_file_setup conf_setup(g_server_configuration_filename);
        advgetopt::conf_file::pointer_t snap_server_conf(advgetopt::conf_file::get_conf_file(conf_setup));
        if(snap_server_conf->get_parameter("backend_status") == "disabled")
        {
            // the administrator disabled all the backends
            //
            return false;
        }

        // okay, now check whether that specific backend is expected to
        // be running on this system because that varies "widely"
        //
        // note: we cache the list of backends once and reuse them as
        //       required (the g_valid_backends variable is static.)
        //
        if(g_valid_backends.empty())
        {
            std::string const expected_backends(snap_server_conf->get_parameter("backends"));
            advgetopt::split_string(expected_backends, g_valid_backends, { "," });
            int const max(g_valid_backends.size());
            for(int idx(0); idx < max; ++idx)
            {
                // in case the admin edited that list manually, we need to
                // fix it before we use it (we should look into using our
                // tokenize_string instead because it can auto-trim, only
                // it uses std::string's)
                //
                g_valid_backends[idx] = snapdev::trim_string(g_valid_backends[idx]);
            }
        }

        // check the status the administrator expects for this backend
        //
        return std::find(
                  g_valid_backends.begin()
                , g_valid_backends.end()
                , f_service) != g_valid_backends.end();
    }

    // else -- this is a service, just not a backend (i.e. snapserver)
    //
    // so a service is expected to be running if enabled and/or active
    //
    return f_service_is_enabled
        || f_service_is_active;
}


/** \brief Wether duplicate definitions are allowed or not.
 *
 * If a process is required by more than one package, then it should
 * be defined in each one of them and it should be marked as a
 * possible duplicate.
 *
 * For example, the mysqld service is required by snaplog and snaplistd.
 * Both will have a defintion for mysqld (because one could be installed
 * on a backend and the other on another backend.) However, when they
 * both get installed on the same machine, you get two definitions with
 * the same process name. If this function returns false for either one,
 * then the setup throws.
 *
 * \return true if the process definitions can have duplicates for that process.
 */
bool sitter_process::allow_duplicates() const
{
    return f_allow_duplicates;
}


/** \brief Match the name and command line against this process definition.
 *
 * If this process is connected to a service, we check whether that service
 * is enabled. If not, then we assume that the user explicitly disabled
 * that service and thus we can't expect the process as running.
 *
 * If we have a command (\<command> tag) then the \p name must match
 * that parameter.
 *
 * If we have a regular expression (\<match> tag), then we match it against
 * the command line (\p cmdline).
 *
 * If there is is no command and no regular expression, then the name of
 * the process is compared directly against the \p command parameter and
 * it has to match that.
 *
 * \param[in] command  The command being checked, this is the command line
 *            very first parameter with the path stripped.
 * \param[in] cmdline  The full command line to compare against the
 *            match regular expression when defined.
 *
 * \return The function returns true if the process is a match.
 */
bool sitter_process::match(std::string const & command, std::string const & cmdline)
{
    if(!f_command.empty())
    {
        if(f_command != command)
        {
            return false;
        }
    }

    if(f_match_defined)
    {
        if(!std::regex_match(cmdline, f_match, std::regex_constants::match_any))
        {
            return false;
        }
    }

    if(f_command.empty()
    && !f_match_defined)
    {
        // if no command line and no match were specified then f_name
        // is the process name
        //
        if(f_name != command)
        {
            return false;
        }
    }

    return true;
}


/** \brief Load a process configuration file.
 *
 * This function loads one configuration file and transform it in a
 * sitter_process object.
 *
 * \param[in] processes_filename  The name of a configuration file
 * representing one process.
 */
void load_process(int index, std::string processes_filename)
{
    snapdev::NOT_USED(index);

    advgetopt::conf_file_setup setup(processes_filename);
    advgetopt::conf_file::pointer_t process(advgetopt::conf_file::get_conf_file(setup));

    if(!process->has_parameter("name"))
    {
        return;
    }
    std::string const name(process->get_parameter("name"));

    bool mandatory(false);
    if(process->has_parameter("mandatory"))
    {
        mandatory = advgetopt::is_true(process->get_parameter("mandatory"));
    }

    bool allow_duplicates(false);
    if(process->has_parameter("allow_duplicates"))
    {
        allow_duplicates = advgetopt::is_true(process->get_parameter("allow_duplicates"));
    }

    auto it(std::find_if(
              g_processes.begin()
            , g_processes.end()
            , [name, allow_duplicates](auto const & wprocess)
            {
                if(name == wprocess.get_name())
                {
                    if(!allow_duplicates
                    || !wprocess.allow_duplicates())
                    {
                        throw invalid_name(
                                  "found process \""
                                + name
                                + "\" twice and duplicates are not allowed.");
                    }
                    return true;
                }
                return false;
            }));
    if(it != g_processes.end())
    {
        // skip the duplicate, we assume that the command,
        // match, etc. are identical enough for the system
        // to still work as expected
        //
        if(mandatory)
        {
            it->set_mandatory(true);
        }
        return;
    }

    sitter_process wp(name, mandatory, allow_duplicates);

    if(process->has_parameter("command"))
    {
        wp.set_command(process->get_parameter("command"));
    }

    if(process->has_parameter("service"))
    {
        bool backend(false);
        if(process->has_parameter("backend"))
        {
            backend = advgetopt::is_true(process->get_parameter("backend"));
        }
        wp.set_service(process->get_parameter("service"), backend);
    }

    if(process->has_parameter("match"))
    {
        wp.set_match(process->get_parameter("match"));
    }

    g_processes.push_back(wp);
}


/** \brief Load the list of watchdog processes.
 *
 * This function loads the XML from the watchdog and other packages.
 *
 * \param[in] processes_path  The path to the list of XML files declaring
 *            processes that should be running.
 */
void load_processes(std::string processes_path)
{
    g_processes.clear();

    // get the path to the processes XML files
    //
    if(processes_path.empty())
    {
        processes_path = "/usr/share/sitter/processes";
    }

    snapdev::glob_to_list<std::list<std::string>> script_filenames;
    script_filenames.read_path<
          snapdev::glob_to_list_flag_t::GLOB_FLAG_NO_ESCAPE
        , snapdev::glob_to_list_flag_t::GLOB_FLAG_IGNORE_ERRORS>(processes_path + "/*.conf");
    snapdev::enumerate(script_filenames, std::bind(&load_process, std::placeholders::_1, std::placeholders::_2));
}


}
// no name namespace





/** \brief Initialize processes.
 *
 * This function terminates the initialization of the processes plugin
 * by registering for various events.
 */
void processes::bootstrap()
{
    SERVERPLUGINS_LISTEN(processes, "server", server, process_watch, boost::placeholders::_1);
}


/** \brief Process this watchdog data.
 *
 * This function runs this watchdog.
 *
 * \param[in] doc  The document.
 */
void processes::on_process_watch(as2js::JSON::JSONValueRef & json)
{
    SNAP_LOG_DEBUG
        << "processes::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    load_processes(plugins()->get_server<sitter::server>()->get_server_parameter(g_sitter_name_processes_processes_path));

    as2js::JSON::JSONValueRef e(json["processes"]);

    cppprocess::process_list list;
    for(auto it(list.begin()); it != list.end() && !g_processes.empty(); ++it)
    {
        std::string name(it->second->get_name());

        // keep the full path in the cmdline parameter
        //
        std::string cmdline(name);

        // compute basename
        //
        std::string::size_type p(name.find_last_of('/'));
        if(p != std::string::npos)
        {
            name = name.substr(p + 1);
        }

        // add command line arguments
        //
        int const count_max(it->second->get_args_size());
        for(int c(0); c < count_max; ++c)
        {
            // skip empty arguments
            //
            if(!it->second->get_arg(c).empty())
            {
                cmdline += ' ';

                // IMPORTANT NOTE: we should escape special characters
                //                 only it would make the command line
                //                 regular expression more complicated
                //
                cmdline += it->second->get_arg(c);
            }
        }
        size_t const max_re(g_processes.size());
//std::cerr << "check process [" << name << "] -> [" << cmdline << "]\n";
        for(size_t j(0); j < max_re; ++j)
        {
            if(g_processes[j].match(name, cmdline))
            {
                plugins()->get_server<sitter::server>()->output_process(
                      "processes"
                    , e
                    , it->second
                    , g_processes[j].get_name()
                    , 35);      // <- priority is not used, the pointer cannot be nullptr

                // for backends we have a special case when they are running,
                // we may actually have them turned off and still running
                // which is not correct
                //
                if(g_processes[j].is_backend()
                && !g_processes[j].is_process_expected_to_run())
                {
                    // TODO: get the correct JSONValue to update (i.e. last
                    //       item of array of processes is the process where
                    //       we need to stick this error)
                    //
                    //e["error"] = "running";

                    plugins()->get_server<sitter::server>()->append_error(
                              e
                            , "processes"
                            , "found process \""
                                + g_processes[j].get_name()
                                + "\" running when disabled."
                            , 35);
                }

                // remove from the list, if the list is empty, we are
                // done; if the list is not empty by the time we return
                // some processes are missing
                //
                g_processes.erase(g_processes.begin() + j);
                break;
            }
        }
    }

    // some process(es) missing?
    //
    size_t const max_re(g_processes.size());
    for(size_t j(0); j < max_re; ++j)
    {
        as2js::JSON::JSONValueRef proc(json["process"][-1]);
        proc["name"] = g_processes[j].get_name();

        if(g_processes[j].is_process_expected_to_run())
        {
            // this process is expected to be running so having
            // found it in this loop, it is an error (missing)
            //
            proc["error"] = "missing";

            // TBD: what should the priority be on this one?
            //      it's likely super important so more than 50
            //      but probably not that important that it should be
            //      close to 100?
            //
            int priority(50);
            std::string message;
            if(g_processes[j].is_mandatory())
            {
                message += "can't find mandatory process \"";
                message += g_processes[j].get_name();
                message += "\" in the list of processes.";
                priority = 95;
            }
            else
            {
                message += "can't find expected process \"";
                message += g_processes[j].get_name();
                message += "\" in the list of processes.";
                priority = 60;
            }

            if(g_processes[j].is_backend()
            && is_in_maintenance())
            {
                // a backend which is not running while we are in
                // maintenance is a very low priority
                //
                priority = 5;
            }

            plugins()->get_server<sitter::server>()->append_error(
                      proc
                    , "processes"
                    , message
                    , priority);
        }
        else
        {
            proc["resident"] = "no";
        }
    }
}



} // namespace processes
} // namespace sitter
// vim: ts=4 sw=4 et
