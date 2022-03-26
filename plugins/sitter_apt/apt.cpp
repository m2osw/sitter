// Copyright (c) 2013-2022  Made to Order Software Corp.  All Rights Reserved.
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
#include    "apt.h"


// snaplogger
//
#include    <snaplogger/message.h>


// snapdev
//
#include    <snapdev/not_used.h>
#include    <snapdev/not_reached.h>


// last include
//
#include    <snapdev/poison.h>



namespace sitter
{
namespace apt
{


CPPTHREAD_PLUGIN_START(apt, 1, 0)
    , ::cppthread::plugin_description(
            "Check the apt-check results. If an update is available, it"
            " will show up as a low priority \"error\" unless it is marked"
            " as a security upgrade.")
    , ::cppthread::plugin_dependency("server")
    , ::cppthread::plugin_help_uri("https://snapwebsites.org/help")
    , ::cppthread::plugin_categorization_tag("packages")
CPPTHREAD_PLUGIN_END(apt)




/** \brief Initialize apt.
 *
 * This function terminates the initialization of the apt plugin
 * by registering for different events.
 *
 * \param[in] sitter  The server handling this request.
 */
void apt::bootstrap(void * server)
{
    f_server = static_cast<server *>(server);

    SNAP_LISTEN(apt, "server", server, process_watch, boost::placeholders::_1);
}


/** \brief Process this watchdog data.
 *
 * This function runs this watchdog.
 *
 * \param[in] doc  The document.
 */
void apt::on_process_watch(as2js::JSON::JSONValueRef & json)
{
    SNAP_LOG_DEBUG
        << "apt::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    as2js::JSON::JSONValueRef & apt(json["apt"]);

    // get path to apt-check file
    //
    // first use the default path
    //
    std::string manager_cache_path("/var/cache/snapwebsites");

    // then check whether the user changed that default in the configuration
    // files
    //
    advgetopt::conf_file_setup setup_config("snapmanager");
    advgetopt::conf_file::pointer_t manager_config(advgetopt::conf_file::get_conf_file(setup_config));
    if(manager_config->has_parameter("cache_path"))
    {
        manager_cache_path = manager_config->get_parameter("cache_path");
    }

    // now define the name we use to save the apt-check output
    //
    std::string const apt_check_output(manager_cache_path + "/apt-check.output");

    // load the apt-check file
    //
    snapdev::file_contents apt_check(apt_check_output);
    if(apt_check.read_all())
    {
        std::string const contents(snapdev::trim_string(apt_check.contents()));
        if(contents == "-1")
        {
            std::string const err_msg(
                    "we are unable to check whether some updates are"
                    " available (the `apt-check` command was not found)");
            apt["error"] = err_msg;
            f_snap->append_error(doc, "apt", err_msg, 98);
            return;
        }

        advgetopt::string_list_t counts;
        advgetopt::split_string(contents, counts, {";"});
        if(counts.size() == 3)
        {
            time_t const now(time(nullptr));

            std::int64_t cached_on;
            advgetopt::validator_integer(counts[0], cached_on);

            // save the date when it was last updated
            //
            apt["last-updated"] = cached_on;

            // out of date tested with a +1h because it could take a little
            // while to check for new updates and the date here is not
            // updated while that happens
            //
            if(cached_on + 86400 + 60 * 60 >= now)
            {
                // cache is still considered valid
                //
                if(counts[1] == "0")
                {
                    // nothing needs to be upgraded
                    //
                    return;
                }

                // counts[1] packages can be upgraded
                //
                std::int64_t count;
                advgetopt::validator_integer(counts[1], count);
                apt["total-updates"] = count;

                // counts[2] are security upgrades
                //
                advgetopt::validator_integer(counts[2], count);
                apt["security-updates"] = count;

                // the following generates an "error" with a low priority
                // (under 50) in case a regular set of files can be upgraded
                // and 52  when there are security updates
                //
                int priority(0);
                std::string err_msg;
                if(counts[2] != "0")
                {
                    priority = 52;
                    err_msg = "there are packages including security updates that need to be upgraded on this system.";
                }
                else
                {
                    priority = 45;
                    err_msg = "there are standard packages that can be upgraded now on this system.";
                }
                apt["error"] = err_msg;
                f_server->append_error(apt, "apt", err_msg, priority);
                return;
            }
            else
            {
                std::string const err_msg(
                          "\""
                        + apt_check_output
                        + "\" file is out of date, the snapmanagerdaemon did not update it for more than a day");
                apt["error"] = err_msg;
                f_server->append_error(apt, "apt", err_msg, 50);
                return;
            }
        }
        else
        {
            // low priority (15): the problem is here but we don't tell the
            //                    admin unless another high level error occurs
            //
            std::string const err_msg(
                      "could not figure out the contents of \""
                    + apt_check_output
                    + "\", snapmanagerdaemon may have changed the format since we wrote the sitter apt plugin?");
            apt["error"] = err_msg;
            f_server->append_error(apt, "apt", err_msg, 15);
            return;
        }
    }
    else
    {
        // when not present, we want to generate an error because that
        // could mean something is wrong on that system, but we make it
        // a low priority for a while (i.e. hitting the Reset button
        // in the snapmanager.cgi interface deletes that file!)
        //
        std::string const err_msg(
                  "\""
                + apt_check_output
                + "\" file is missing, sitter is not getting APT status updates from snapmanagerdaemon");
        apt["error"] = err_msg;
        f_server->append_error(apt, "apt", err_msg, 20);
        return;
    }
}



} // namespace apt
} // namespace sitter
// vim: ts=4 sw=4 et
