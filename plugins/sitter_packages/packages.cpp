// Copyright (c) 2013-2022  Made to Order Software Corp.  All Rights Reserved
//
// https://snapwebsites.org/project/sitter
// contact@m2osw.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


// self
//
#include    "packages.h"

#include    "names.h"



// sitter
//
#include    <sitter/exception.h>


// advgetopt
//
#include    <advgetopt/conf_file.h>
#include    <advgetopt/validator_integer.h>


// snaplogger
//
#include    <snaplogger/message.h>


// cppprocess
//
#include    <cppprocess/process.h>
#include    <cppprocess/io_capture_pipe.h>


// snapdev
//
#include    <snapdev/enumerate.h>
#include    <snapdev/file_contents.h>
#include    <snapdev/glob_to_list.h>
#include    <snapdev/join_strings.h>
#include    <snapdev/not_reached.h>
#include    <snapdev/not_used.h>
#include    <snapdev/trim_string.h>


// serverplugins
//
#include    <serverplugins/collection.h>


// C++
//
#include    <sstream>


// last include
//
#include    <snapdev/poison.h>




/** \file
 * \brief Verify that packages are installed, not installed, not in conflicts.
 *
 * This Sitter plugin checks packages for:
 *
 * \li Packages that are expected to be installed (necessary for Snap! or
 *     enhance security)
 * \li Packages that should not be installed (security issues)
 * \li Packages that are in conflict (i.e. ntpd vs ntpdate)
 *
 * The plugin generates errors in all those situations.
 *
 * For example, if you have ntpd and ntpdate both installed on the
 * same system, they can interfere. Especially, the ntpd daemon may
 * not get restarted while ntpdate is running. If that happens
 * _simultaneously_, then the ntpd can't be restarted and the clock
 * is going to be allowed to drift.
 *
 * This packages plugin expects a list of configuration files with
 * definitions of packages as defined above: required, unwanted, in
 * conflict. It is just too hard to make sure invalid installations
 * won't ever happen without help from the computer.
 */

namespace sitter
{
namespace packages
{


SERVERPLUGINS_START(packages, 1, 0)
    , ::serverplugins::description(
          "Check whether a some required packages are missing,"
          " some installed packages are unwanted (may cause problems"
          " with running Snap! or are known security risks,)"
          " or packages that are in conflict.")
    , ::serverplugins::dependency("server")
    , ::serverplugins::help_uri("https://snapwebsites.org/help")
    , ::serverplugins::categorization_tag("security")
    , ::serverplugins::categorization_tag("packages")
SERVERPLUGINS_END(packages)






namespace
{



/** \brief Class used to read the list of packages to check.
 *
 * This class holds one package definition as read from a Unix like
 * configuration file.
 *
 * The class understands the following definitions:
 *
 * \code
 *     name=<package-name>
 *     priority=<priority>
 *     installation=<optional|required|unwanted>
 *     description="<description>"
 *     conflicts=<package-name>[,...]
 * \endcode
 *
 * The `priority` parameter is the priority used to send an error message.
 * A higher priority is more likely to generate an email that gets sent
 * to the administrator.
 *
 * A `package-name` characters are limited to `[-+.:a-z0-9]+`. The name
 * must start with a letter. It can end with a letter or a digit.
 *
 * The `conflicts` parameter defines one or more package names that
 * cannot be installed along this package (i.e. `ntp` vs `ntpdate`).
 * Separate multiple names with commas.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
class sitter_package_t
{
public:
    typedef std::vector<sitter_package_t>               vector_t;
    typedef std::set<std::string>                       package_name_set_t;
    typedef std::map<std::string, bool>                 installed_packages_t;

    enum class installation_t
    {
        PACKAGE_INSTALLATION_OPTIONAL,
        PACKAGE_INSTALLATION_REQUIRED,
        PACKAGE_INSTALLATION_UNWANTED
    };

                                sitter_package_t(
                                          sitter::server::pointer_t snap
                                        , std::string const & name
                                        , installation_t installation
                                        , int priority);

    void                        set_description(std::string const & description);
    void                        add_conflict(std::string const & package_name);

    std::string const &         get_name() const;
    installation_t              get_installation() const;
    std::string                 get_installation_as_string() const;
    std::string const &         get_description() const;
    package_name_set_t const &  get_conflicts() const;
    package_name_set_t const &  get_packages_in_conflict() const;
    int                         get_priority() const;

    bool                        is_package_installed(std::string const & package_name);
    bool                        is_in_conflict();

    static installation_t       installation_from_string(std::string const & installation);

private:
    sitter::server::pointer_t   f_server = sitter::server::pointer_t();
    std::string                 f_name = std::string();
    std::string                 f_description = std::string();
    package_name_set_t          f_conflicts = package_name_set_t();
    package_name_set_t          f_in_conflict = package_name_set_t();
    installation_t              f_installation = installation_t::PACKAGE_INSTALLATION_OPTIONAL;
    int                         f_priority = 15;
};
#pragma GCC diagnostic pop

sitter_package_t::vector_t                  g_packages = sitter_package_t::vector_t();
sitter_package_t::installed_packages_t      g_installed_packages = sitter_package_t::installed_packages_t();
bool                                        g_cache_loaded = false;
bool                                        g_cache_modified = false;


/** \brief Initializes a sitter_package_t object.
 *
 * This function initializes the sitter_package_t making it
 * ready to run the match() command.
 *
 * \note
 * The \p name is like a brief _description_ of the conflict.
 *
 * \param[in] server  The pointer back to the server object.
 * \param[in] name  The name of the package conflict.
 * \param[in] priority  The priority to use in case of conflict.
 */
sitter_package_t::sitter_package_t(
          sitter::server::pointer_t server
        , std::string const & name
        , installation_t installation
        , int priority)
    : f_server(server)
    , f_name(name)
    , f_installation(installation)
    , f_priority(priority)
{
}


/** \brief Set the description of the expected package.
 *
 * This function saves the description of the conflict in more details than
 * its name.
 *
 * \param[in] description  The description of this conflict.
 */
void sitter_package_t::set_description(std::string const & description)
{
    f_description = snapdev::trim_string(description);
}


/** \brief Add the name of an package in conflict with this package.
 *
 * This function takes the name of a package that is in conflict with
 * this package (i.e. see get_name().)
 *
 * Any number of packages in conflict can be added. The only restriction
 * is that a package cannot be in conflict with itself or its dependencies
 * although we do not check all the dependencies (too much work/too slow)
 * as we expect that you will create sensible XML definitions that do not
 * create impossible situations for your users.
 *
 * \param[in] package_name  The name of the package to seek.
 */
void sitter_package_t::add_conflict(std::string const & package_name)
{
    if(package_name == f_name)
    {
        throw invalid_parameter("a package cannot be in conflict with itself");
    }

    f_conflicts.insert(package_name);
}


/** \brief Get the name of the package concerned.
 *
 * This function returns the name of the package concerned by this
 * definition. This is the exact name of a Debian package.
 *
 * \return The name this package.
 */
std::string const & sitter_package_t::get_name() const
{
    return f_name;
}


/** \brief Get the name of this conflict.
 *
 * This function returns the name of the package conflict. This is expected
 * to be a really brief description of the conflict so we know what is
 * being tested.
 *
 * \return The name this package conflict.
 */
sitter_package_t::installation_t sitter_package_t::get_installation() const
{
    return f_installation;
}


/** \brief Get the installation check as a string.
 *
 * This function transforms the installation_t of this package definition
 * in a string for display or saving to file.
 *
 * \return A string representing this package installation mode.
 */
std::string sitter_package_t::get_installation_as_string() const
{
    switch(f_installation)
    {
    case installation_t::PACKAGE_INSTALLATION_REQUIRED:
        return "required";

    case installation_t::PACKAGE_INSTALLATION_UNWANTED:
        return "unwanted";

    //case installation_t::PACKAGE_INSTALLATION_OPTIONAL:
    default:
        return "optional";

    }
}


/** \brief Get the description of this conflict.
 *
 * This function returns the description of the package error. This
 * is used whenever an error message is generated, along the priority
 * and list of packages in conflict if any.
 *
 * \return The description to generate errors.
 */
std::string const & sitter_package_t::get_description() const
{
    return f_description;
}


/** \brief Get the set of conflicts.
 *
 * This function returns the list of package names as defined by the
 * \<conflict> tag in the source XML files.
 *
 * When this package is installed and any one of the conflict package
 * is installed, then an error is generated.
 *
 * \note
 * To declare a package that should never be installed, in conflict or
 * not, you should instead use the "unwanted" installation type and
 * not mark it as in conflict of another package.
 *
 * \return The set of package names considered in conflict with this package.
 */
sitter_package_t::package_name_set_t const & sitter_package_t::get_conflicts() const
{
    return f_conflicts;
}


/** \brief Get the set of packages that are in conflict.
 *
 * This function returns a reference to the set of packages that are in
 * conflict as determined by the is_in_conflict() function. If the
 * is_in_conflict() function was not called yet, then this function
 * always returns an empty set. If the is_in_conflict() function was
 * called and it returned true, then this function always return a
 * set with at least one element.
 *
 * \return The set of package in conflict with the expected package.
 */
sitter_package_t::package_name_set_t const & sitter_package_t::get_packages_in_conflict() const
{
    return f_in_conflict;
}


/** \brief Get the priority of a package conflict object.
 *
 * Whenever an error occurs, the package conflict defines a priority
 * which is to be used with the error generator. This allows various
 * conflicts to be given various level of importance.
 *
 * The default priorty is 15.
 *
 *
 * Remember that to generate an email, the priority needs to be at least
 * 50. Any priority under 50 will still generate an error in the
 * snapmanager.cgi output.
 *
 * \return The priority of this package conflict.
 */
int sitter_package_t::get_priority() const
{
    return f_priority;
}


/** \brief Check whether the specified package is installed.
 *
 * This function returns true if the named package is installed.
 *
 * \return true if the named package is installed.
 */
bool sitter_package_t::is_package_installed(std::string const & package_name)
{
    bool result(false);

    if(!g_cache_loaded)
    {
        g_cache_loaded = true;

        std::string const packages_filename(f_server->get_cache_path(g_name_packages_cache_filename));
        snapdev::file_contents in(packages_filename);
        if(in.read_all())
        {
            std::string const content(in.contents());
            std::string::size_type pos(0);
            while(pos != std::string::npos)
            {
                std::string::size_type s(pos);
                pos = content.find('\n', pos);
                std::string line;
                if(pos == std::string::npos)
                {
                    line = content.substr(s);
                }
                else
                {
                    line = content.substr(s, pos - s);
                    ++pos;  // skip the '\n'
                }
                std::string::size_type equal(line.find('='));
                if(equal != std::string::npos)
                {
                    std::string const name(line.substr(0, equal));
                    if(!name.empty())
                    {
                        std::string const value(line.substr(equal + 1));
                        g_installed_packages[name] = value == "t";
                    }
                }
            }
        }
    }

    auto it(g_installed_packages.find(package_name));
    if(it != g_installed_packages.end())
    {
        // already determined, return the cached result
        //
        result = it->second;
    }
    else
    {
        // get the system status now
        //
        cppprocess::process p("query package status");
        p.set_command("dpkg-query");
        p.add_argument("--showformat='${Status}'");
        p.add_argument("--show");
        p.add_argument(package_name);
        cppprocess::io_capture_pipe::pointer_t out(std::make_shared<cppprocess::io_capture_pipe>());
        p.set_output_io(out);
        int r(p.start());
        if(r == 0)
        {
            r = p.wait();
        }

SNAP_LOG_TRACE
<< "output of dpkg-query is: "
<< r
<< " -> "
<< out->get_trimmed_output()
<< SNAP_LOG_SEND;
        if(r == 0)
        {
            std::string const output(out->get_trimmed_output());
            result = output == "install ok installed";
SNAP_LOG_TRACE
<< "output: ["
<< output
<< "] -> "
<< (result ? "TRUE" : "FALSE")
<< SNAP_LOG_SEND;
        }

        // cache the result in case the same package is checked multiple
        // times...
        //
        g_installed_packages[package_name] = result;

        g_cache_modified = true;
    }

    return result;
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
bool sitter_package_t::is_in_conflict()
{
    // if the expected package is not even installed, there cannot be
    // a conflict because of this definition so ignore the list of
    // unexpected packages
    //
    if(!is_package_installed(f_name))
    {
        return false;
    }

    f_in_conflict.clear();
    for(auto u : f_conflicts)
    {
        if(is_package_installed(u))
        {
            // we are in conflict at least with this package
            //
            f_in_conflict.insert(u);
        }
    }

    return !f_in_conflict.empty();
}


/** \brief Transform a string in an installation type.
 *
 * This function converts a string in an initialization_t enumeration type.
 *
 * \exception invalid_argument
 * If the string is not recognized or empty, then this exception is raised.
 *
 * \note
 * The exception should never occur if the file gets checked for validity
 * with the XSD before it is used by Snap!
 *
 * \param[in] installation  The installation to convert to an enumeration.
 *
 * \return The corresponding installation type.
 */
sitter_package_t::installation_t sitter_package_t::installation_from_string(std::string const & installation)
{
    if(installation.empty()
    || installation == "optional")
    {
        return installation_t::PACKAGE_INSTALLATION_OPTIONAL;
    }
    if(installation == "required")
    {
        return installation_t::PACKAGE_INSTALLATION_REQUIRED;
    }
    if(installation == "unwanted")
    {
        return installation_t::PACKAGE_INSTALLATION_UNWANTED;
    }

    throw invalid_parameter("invalid installation name, cannot load your configuration file");
}



/** \brief Save the cache if it was updated.
 *
 * This function updates the cache used to save the installed packages
 * in the event it was modified (i.e. a new package was listed somewhere.)
 *
 * The cache is used so we can still check for the conflicts once a minute
 * but not spend the time it takes (very long) to check whether each package
 * is or is not installed.
 *
 * The cache gets reset once a day so it can be redefined anew at that time
 * and a new status determined.
 *
 * \param[in] server  A pointer to the child so we can get the path to the cache.
 */
void save_cache(sitter::server::pointer_t server)
{
    if(g_cache_modified)
    {
        std::string const packages_filename(server->get_cache_path(g_name_packages_cache_filename));
        std::ofstream out(packages_filename);
        for(auto const & p : g_installed_packages)
        {
            out << p.first
                << '='
                << (p.second ? 't' : 'f')
                << '\n';
        }
    }
}




}
// no name namespace




/** \brief Initialize packages.
 *
 * This function terminates the initialization of the packages plugin
 * by registering for various events.
 */
void packages::bootstrap()
{
    SERVERPLUGINS_LISTEN(packages, "server", server, process_watch, boost::placeholders::_1);
}


/** \brief Process this sitter data.
 *
 * This function runs this plugin actual check.
 *
 * \param[in] json  The document where the results are collected.
 */
void packages::on_process_watch(as2js::JSON::JSONValueRef & json)
{
    SNAP_LOG_DEBUG
        << "packages::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    load_packages();

    as2js::JSON::JSONValueRef e(json["packages"]);

SNAP_LOG_TRACE
<< "got "
<< g_packages.size()
<< " packages to check..."
<< SNAP_LOG_SEND;
    for(auto pc : g_packages)
    {
        as2js::JSON::JSONValueRef package(e["package"][-1]);

        std::string const name(pc.get_name());
        package["name"] = name;
        package["installation"] = pc.get_installation_as_string();
        auto const & possible_conflicts(pc.get_conflicts());
        if(!possible_conflicts.empty())
        {
            package["conflicts"] = snapdev::join_strings(possible_conflicts, ", ");
        }

        sitter_package_t::installation_t const installation(pc.get_installation());
        switch(installation)
        {
        case sitter_package_t::installation_t::PACKAGE_INSTALLATION_REQUIRED:
            if(!pc.is_package_installed(pc.get_name()))
            {
                // package is required, so it is in error if not installed
                //
                plugins()->get_server<sitter::server>()->append_error(
                          package
                        , "packages"
                        , "The \""
                          + name
                          + "\" package is required but not (yet) installed."
                            " Please install this package at your earliest convenience."
                        , pc.get_priority());

                // continue for loop
                //
                continue;
            }
            break;

        case sitter_package_t::installation_t::PACKAGE_INSTALLATION_UNWANTED:
            if(pc.is_package_installed(pc.get_name()))
            {
                // package is unwanted, so it should not be installed
                //
                plugins()->get_server<sitter::server>()->append_error(
                          package
                        , "packages"
                        , "The \""
                          + name
                          + "\" package is expected to NOT ever be installed."
                            " Please remove this package at your earliest convenience."
                        , pc.get_priority());

                // continue for loop
                //
                continue;
            }
            break;

        // optional means that it may or may not be installed
        //case sitter_package::installation_t::PACKAGE_INSTALLATION_OPTIONAL:
        default:
            break;

        }

        if(pc.is_in_conflict())
        {
            // conflict discovered, generate an error
            //
            auto const & conflicts(pc.get_packages_in_conflict());
            std::string const conflicts_list(snapdev::join_strings(conflicts, "\", \""));

            std::stringstream ss;

            ss << pc.get_description()
               << " The \""
               << pc.get_name()
               << "\" package is in conflict with \""
               << conflicts_list
               << "\".";

            plugins()->get_server<sitter::server>()->append_error(
                      json
                    , "packages"
                    , ss.str()
                    , pc.get_priority());
        }
        // else -- everything's fine
    }

    // the cache may have been modified, save it if so
    //
    save_cache(plugins()->get_server<sitter::server>());
}


/**\brief Load the list of sitter packages.
 *
 * This function loads the configuration files from the sitter and other
 * packages that define packages that are to be reported to the administrator.
 */
void packages::load_packages()
{
    g_packages.clear();

    // get the path to the packages configuration files
    //
    std::string packages_path(plugins()->get_server<sitter::server>()->get_server_parameter(g_name_packages_path));
    if(packages_path.empty())
    {
        packages_path = "/usr/share/sitter/packages";
    }
    SNAP_LOG_TRACE
        << "load package files from "
        << packages_path
        << "..."
        << SNAP_LOG_SEND;

    // parse every configuration file
    //
    snapdev::glob_to_list<std::vector<std::string>> script_filenames;
    script_filenames.read_path<
          snapdev::glob_to_list_flag_t::GLOB_FLAG_NO_ESCAPE
        , snapdev::glob_to_list_flag_t::GLOB_FLAG_IGNORE_ERRORS>(packages_path + "/*.conf");
    snapdev::enumerate(
              script_filenames
            , std::bind(&packages::load_package, this, std::placeholders::_1, std::placeholders::_2));
}


/** \brief Load a package configuration file.
 *
 * This function loads one configuration file and transform it in a
 * sitter_package_t object.
 *
 * \exception invalid_name
 * This exception is raised if the name from a package is empty
 * or undefined.
 *
 * \param[in] index  The index of the file to load.
 * \param[in] package_filename  The name of an XML file representing
 *                              required, unwanted, or conflicted packages.
 */
void packages::load_package(int index, std::string package_filename)
{
    snapdev::NOT_USED(index);

    advgetopt::conf_file_setup setup(package_filename);
    advgetopt::conf_file::pointer_t package(advgetopt::conf_file::get_conf_file(setup));

    if(!package->has_parameter("name"))
    {
        return;
    }

    std::string const name(package->get_parameter("name"));

    std::int64_t priority(15);
    if(package->has_parameter("priority"))
    {
        std::string const priority_str(package->get_parameter("priority"));
        advgetopt::validator_integer::convert_string(priority_str, priority);
    }

    sitter_package_t::installation_t installation(sitter_package_t::installation_t::PACKAGE_INSTALLATION_OPTIONAL);
    if(package->has_parameter("intallation"))
    {
        installation = sitter_package_t::installation_from_string(package->get_parameter("installation"));
    }

    std::string description;
    if(package->has_parameter("description"))
    {
        description = package->get_parameter("description");
    }

    advgetopt::string_list_t conflicts;
    if(package->has_parameter("conflicts"))
    {
        advgetopt::split_string(
                  package->get_parameter("conflicts")
                , conflicts
                , { "," });
    }

    sitter_package_t wp(
              plugins()->get_server<sitter::server>()
            , name
            , installation
            , priority);

    wp.set_description(description);

    for(auto c : conflicts)
    {
        wp.add_conflict(c);
    }

    g_packages.push_back(wp);
}



} // namespace packages
} // namespace sitter
// vim: ts=4 sw=4 et
