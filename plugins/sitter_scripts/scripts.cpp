// Snap Websites Server -- watchdog scripts
// Copyright (c) 2018-2019  Made to Order Software Corp.  All Rights Reserved
//
// https://snapwebsites.org/
// contact@m2osw.com
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
#include    "scripts.h"


// sitter
//
#include    <sitter/version.h>
#include    <sitter/exception.h>


// libmimemail
//
#include    <libmimemail/email.h>


// cppprocess
//
#include    <cppprocess/io_capture_pipe.h>


// advgetopt
//
#include    <advgetopt/conf_file.h>


// snaplogger
//
#include    <snaplogger/message.h>


// snapdev lib
//
#include    <snapdev/enumerate.h>
#include    <snapdev/file_contents.h>
#include    <snapdev/glob_to_list.h>
#include    <snapdev/not_used.h>
#include    <snapdev/not_reached.h>


// libaddr lib
//
#include    <libaddr/addr.h>
#include    <libaddr/iface.h>


// last include
//
#include    <snapdev/poison.h>



namespace sitter
{
namespace scripts
{



CPPTHREAD_PLUGIN_START(scripts, 1, 0)
    , ::cppthread::plugin_description(
            "Check whether a set of scripts are running.")
    , ::cppthread::plugin_dependency("server")
    , ::cppthread::plugin_help_uri("https://snapwebsites.org/help")
    , ::cppthread::plugin_categorization_tag("custom")
    , ::cppthread::plugin_categorization_tag("script")
CPPTHREAD_PLUGIN_END(scripts)



/** \brief Get a fixed scripts plugin name.
 *
 * The scripts plugin makes use of different names. This function ensures
 * that you get the right spelling for a given name.
 *
 * \param[in] name  The name to retrieve.
 *
 * \return A pointer to the name.
 */
char const * get_name(name_t name)
{
    switch(name)
    {
    case name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_DEFAULT_LOG_SUBFOLDER:
        return "snapwatchdog-output";

    case name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_LOG_SUBFOLDER:
        return "log_subfolder";

    case name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_OUTPUT:
        return "watchdog_watchscripts_output";

    case name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_OUTPUT_DEFAULT:
        return "/var/lib/snapwebsites/snapwatchdog/script-files";

    case name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_PATH:
        return "watchdog_watchscripts_path";

    case name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_PATH_DEFAULT:
        return "/usr/share/snapwebsites/snapwatchdog/scripts";

    case name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_WATCH_SCRIPT_STARTER:
        return "watch_script_starter";

    case name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_WATCH_SCRIPT_STARTER_DEFAULT:
        return "/usr/sbin/watch_script_starter";

    default:
        // invalid index
        throw snap_logic_error("Invalid SNAP_NAME_WATCHDOG_WATCHSCRIPTS_...");

    }
    snapdev::NOT_REACHED();
}


/** \brief Initialize scripts.
 *
 * This function terminates the initialization of the scripts plugin
 * by registering for various events.
 *
 * \param[in] snap  The child handling this request.
 */
void scripts::bootstrap(void * s)
{
    f_server = static_cast<server *>(s);

    SNAP_LISTEN(scripts, "server", server, process_watch, boost::placeholders::_1);

    f_watch_script_starter = [&]()
        {
            std::string const starter(f_snap->get_server_parameter(get_name(name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_WATCH_SCRIPT_STARTER)));
            if(starter.empty())
            {
                return get_name(name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_WATCH_SCRIPT_STARTER_DEFAULT);
            }
            return starter;
        }();

    // setup a variable that our scripts can use to save data as they
    // see fit; especially, many scripts need to remember what they've
    // done before or maybe they don't want to run too often and use a
    // file to know when to run again
    //
    std::string const scripts_output([&]()
        {
            std::string const filename(f_snap->get_server_parameter(get_name(name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_OUTPUT)));
            if(filename.empty())
            {
                return get_name(name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_OUTPUT_DEFAULT);
            }
            return filename;
        }());
    setenv("WATCHDOG_WATCHSCRIPTS_OUTPUT", scripts_output.c_str());

    f_log_path = [&]()
        {
            std::string const path(f_snap->get_server_parameter(get_name(watchdog::name_t::SNAP_NAME_WATCHDOG_LOG_PATH)));
            if(path.empty())
            {
                return get_name(watchdog::name_t::SNAP_NAME_WATCHDOG_DEFAULT_LOG_PATH);
            }
            return path;
        }();
    setenv("WATCHDOG_WATCHSCRIPTS_LOG_PATH", f_log_path.c_str(), 1);

    f_log_subfolder = [&]()
        {
            std::string const folder(f_snap->get_server_parameter(get_name(name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_LOG_SUBFOLDER)));
            if(folder.isEmpty())
            {
                return get_name(name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_DEFAULT_LOG_SUBFOLDER);
            }
            return folder;
        }();
    setenv("WATCHDOG_WATCHSCRIPTS_LOG_SUBFOLDER", f_log_subfolder.c_str(), 1);

    f_scripts_output_log = f_log_path + "/" + f_log_subfolder + "/snapwatchdog-scripts.log";
    f_scripts_error_log = f_log_path + "/" + f_log_subfolder + "/snapwatchdog-scripts-errors.log";
}


/** \brief Process this watchdog data.
 *
 * This function runs this watchdog.
 *
 * The process is to go through all the script in the snapwatchdog directory
 * and run them. If they exit with 2, then they detected a problem and we
 * send an email to the administrator. If they exit with 1, the script is
 * bogus and we send an email to the administrator. If they exit with 0,
 * no problem was discovered yet.
 *
 * The scripts are standard shell scripts. The snapwatchdog environment
 * offers additional shell commands, though, to ease certain things that
 * are otherwise very complicated.
 *
 * The results are also saved in the `doc` XML data.
 *
 * \param[in] doc  The document.
 */
void scripts::on_process_watch(QDomDocument doc)
{
    SNAP_LOG_DEBUG
        << "scripts::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    QString const scripts_path([&]()
        {
            QString const path(f_snap->get_server_parameter(get_name(name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_PATH)));
            if(path.isEmpty())
            {
                return QString::fromUtf8(get_name(name_t::SNAP_NAME_WATCHDOG_WATCHSCRIPTS_PATH_DEFAULT));
            }
            return path;
        }());

    QDomElement parent(snap_dom::create_element(doc, "watchdog"));
    f_watchdog = snap_dom::create_element(parent, "scripts");

    // allow for failures, admins are responsible for making sure it will
    // work as expected
    //
    f_output_file.reset(new QFile(f_scripts_output_log));
    if(f_output_file != nullptr
    && !f_output_file->open(QIODevice::Append))
    {
        f_output_file.reset();
    }
    f_error_file.reset(new QFile(f_scripts_error_log));
    if(f_error_file != nullptr
    && !f_error_file->open(QIODevice::Append))
    {
        f_error_file.reset();
    }

    snapdev::glob_to_list<std::list<std::string>> script_filenames;
    script_filenames.read_path<
          snapdev::glob_to_list_flag_t::GLOB_FLAG_NO_ESCAPE
        , snapdev::glob_to_list_flag_t::GLOB_FLAG_EMPTY>(std::string(scripts_path.toUtf8().data()) + "/*");
    snapdev::enumerate(
              script_filenames
            , std::bind(
                  &scripts::process_script
                , this
                , std::placeholders::_1
                , std::placeholders::_2));

    // release memory (it could be somewhat large)
    //
    f_output.clear();
    f_error.clear();
}


void scripts::process_script(int index, std::string script_filename)
{
    snapdev::NOT_USED(index);

    // skip any README file
    //
    // (specifically, we install a file named watchdogscripts_README.md
    // in the folder as a placeholder with documentation)
    //
    if(script_filename.find("README") != std::string::npos)
    {
        return;
    }

    // setup the variable used while running a script
    //
    f_new_output_script = true;
    f_new_error_script = true;
    f_last_output_byte = '\n'; // whatever works in here, but I think this '\n' makes it clearer
    f_last_error_byte = '\n';

    f_output.clear();
    f_error.clear();
    f_script_filename = QString::fromUtf8(script_filename.c_str());
    f_start_date = time(nullptr);

    // run the script
    //
    cppprocess::process p("watchscript");

    // Note: scripts that do not have the execution permission set are
    //       started with /bin/sh
    //
    p.set_command(f_watch_script_starter.toUtf8().data());
    p.add_argument(script_filename);

    cppprocess::io_capture_pipe::pointer_t out(std::make_shared<cppprocess::io_capture_pipe>());
    p.set_output_io(out);

    cppprocess::io_capture_pipe::pointer_t err(std::make_shared<cppprocess::io_capture_pipe>());
    p.set_error_io(err);

    int exit_code(p.start());
    if(exit_code == 0)
    {
        exit_code = p.wait();
    }
    std::string const output(out->get_trimmed_output());

    //p.set_output_callback(this);

    QDomDocument doc(f_watchdog.ownerDocument());
    QDomElement script(doc.createElement("script"));
    f_watchdog.appendChild(script);

    script.setAttribute("name", QString::fromUtf8(script_filename.c_str()));
    script.setAttribute("exit_code", exit_code);

    // if we output some data and it did not end with \n then add it now
    //
    if(!f_new_output_script
    && f_last_output_byte != '\n'
    && f_output_file != nullptr)
    {
        f_output_file->write("\n", 1);
        f_output += "\n";
    }
    if(!f_new_error_script
    && f_last_error_byte != '\n'
    && f_error_file != nullptr)
    {
        f_error_file->write("\n", 1);
        f_error += "\n";
    }

    SNAP_LOG_TRACE
        << "script \""
        << script_filename
        << "\" exited with "
        << exit_code
        << ", and "
        << f_output.length()
        << " bytes of output and "
        << f_error.length()
        << " bytes of error."
        << SNAP_LOG_SEND;

    if(exit_code == 0
    && !f_error.isEmpty())
    {
        SNAP_LOG_WARNING
            << "we got errors but the process exit code is 0"
            << SNAP_LOG_SEND;
    }

    // if we received some output, email it to the administrator
    // if we also had a failing script
    //
    if(exit_code != 0
    && !f_output.isEmpty())
    {
        QDomElement output_tag(doc.createElement("output"));
        script.appendChild(output_tag);
        QDomText text(doc.createTextNode(f_output));
        output_tag.appendChild(text);

        f_snap->append_error(doc, "scripts", f_output, 35);
    }
    if(!f_error.isEmpty())
    {
        QDomElement output_tag(doc.createElement("error"));
        script.appendChild(output_tag);
        QDomText text(doc.createTextNode(f_error));
        output_tag.appendChild(text);

        f_snap->append_error(doc, "scripts", f_error, 90);
    }
}


/** \brief Generate the output or error message header.
 *
 * The function generates an email like header for the output or
 * error message. The header includes information aboutwhen the
 * output was generated, which script it is wrong, which
 * version of the snapwatchdog it comes from and an IP address.
 *
 * \param[in] type  The type of header (Output or Error)
 *
 * \return The header as a string.
 */
std::string scripts::generate_header(std::string const & type)
{
    std::string header = std::string("--- %1 -----------------------------------------------------------\n"
                     "Snap-Watchdog-Version: " SNAPWATCHDOG_VERSION_STRING "\n"
                     "Output-Type: %1\n"
                     "Date: %2\n"
                     "Script: %3\n")
                            .arg(type)
                            .arg(format_date(f_start_date))
                            .arg(f_script_filename);

    // TODO: see whether we should instead use snapdev::get_hostname()
    snapdev::file_contents hostname("/etc/hostname");
    if(hostname.read_all())
    {
        header += "Hostname: ";
        header += snapdev::trim_string(hostname.contents())
        header += "\n";
    }

    // if we have a properly installed snapcommunicator use that IP
    //
    advgetopt::conf_file_setup setup("/etc/snapwebsites/snapcommunicator.conf");
    advgetopt::conf_file::pointer_t config(advgetopt::conf_file::get_conf_file(setup));
    std::string const my_ip(config->get_parameter("my_address"));
    if(!my_ip.empty())
    {
        header += "IP-Address: ";
        header += my_ip;
        header += "\n";
    }
    else
    {
        // no snapcommunicator defined "my_address", then show
        // all the IPs on this computer
        //
        addr::iface::vector_t const ips(addr::iface::get_local_addresses());
        if(!ips.empty())
        {
            header += "IP-Addresses: ";
            std::string sep;
            for(auto const & i : ips)
            {
                header += sep;
                header += i.get_address().to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_BRACKETS);
                sep = ", ";
            }
            header += '\n';
        }
    }

    header += '\n';

    return header;
}


//bool scripts::output_available(cppprocess::io * io, cppprocess::done_reason_t reason)
//{
//    // ignore if empty (it should not happen but our code depends on that premise.)
//    //
//    if(output.isEmpty())
//    {
//        return true;
//    }
//
//    // generate a line to seperate each script entry
//    //
//    QString header;
//    if(f_new_output_script)
//    {
//        header = generate_header("OUTPUT");
//
//        // we can immediately add it to the output buffer
//        //
//        f_output += header;
//    }
//    f_output += QString::fromUtf8(output);
//
//    // if there is an output file, write that output data to it
//    //
//    if(f_output_file != nullptr)
//    {
//        // first write for this script? then write its name
//        //
//        if(f_new_output_script)
//        {
//            f_output_file->write(header.toUtf8());
//        }
//        f_output_file->write(output);
//
//        // save the last byte so we know whether we had a "\n"
//        //
//        f_last_output_byte = output.at(output.length() - 1);
//    }
//
//    f_new_output_script = false;
//
//    return true;
//}


//bool scripts::error_available(cppprocess::io * io, cppprocess::done_reason_t reason)
//{
//    // ignore if empty (it should not happen but our code depends on it.)
//    //
//    if(error.isEmpty())
//    {
//        return true;
//    }
//
//    // generate a line to seperate each script entry
//    //
//    QString header;
//    if(f_new_error_script)
//    {
//        header = generate_header("ERROR");
//
//        // we can immediately add it to the error buffer
//        //
//        f_error += header;
//    }
//    f_error += QString::fromUtf8(error);
//
//    // if there is an error output file, write that error data to it
//    //
//    if(f_error_file != nullptr)
//    {
//        // first write for this script? then write its name
//        //
//        if(f_new_error_script)
//        {
//            f_error_file->write(header.toUtf8());
//        }
//        f_error_file->write(error);
//
//        // save the last byte so we know whether we had a "\n"
//        //
//        f_last_error_byte = error.at(error.length() - 1);
//    }
//
//    f_new_error_script = false;
//
//    return true;
//}


// TODO: move that to our edhttp http_date implementation
std::string scripts::format_date(time_t const t)
{
    struct tm f;
    gmtime_r(&t, &f);
    char date[32]; // YYYY/MM/DD HH:MM:SS (19 characters)
    strftime(date, sizeof(date) - 1, "%D %T", &f);
    return date;
}



} // namespace scripts
} // namespace sitter
// vim: ts=4 sw=4 et
