// Copyright (c) 2013-2025  Made to Order Software Corp.  All Rights Reserved.
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
#include    "reboot.h"


// sitter
//
#include    <sitter/sys_stats.h>


// libexcept
//
#include    <libexcept/exception.h>


// snaplogger
//
#include    <snaplogger/message.h>


// advgetopt
//
#include    <advgetopt/validator_integer.h>


// serverplugins
//
#include    <serverplugins/collection.h>


// snapdev
//
#include    <snapdev/file_contents.h>
#include    <snapdev/not_reached.h>
#include    <snapdev/not_used.h>


// C++
//
#include    <thread>


// last include
//
#include    <snapdev/poison.h>



namespace sitter
{
namespace reboot
{

SERVERPLUGINS_START(reboot)
    , ::serverplugins::description(
            "Check for the /run/reboot-required flag and raise one of our flags if set.")
    , ::serverplugins::dependency("server")
    , ::serverplugins::help_uri("https://snapwebsites.org/help")
    , ::serverplugins::categorization_tag("os")
SERVERPLUGINS_END(reboot)






/** \brief Initialize the reboot plugin.
 *
 * This function terminates the initialization of the reboot plugin
 * by registering for different events.
 */
void reboot::bootstrap()
{
    SERVERPLUGINS_LISTEN(reboot, server, process_watch, std::placeholders::_1);
}


/** \brief Process the reboot plugin.
 *
 * This function checks whether the "/run/reboot-required" flag is set.
 * If so, then we generate an error about the state.
 *
 * The priority changes depending on how long it has been in that
 * state.
 *
 * \param[in] json  The document where the results are collected.
 */
void reboot::on_process_watch(as2js::json::json_value_ref & json)
{
    SNAP_LOG_DEBUG
        << "reboot::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    as2js::json::json_value_ref e(json["reboot"]);

    bool const required(access("/run/reboot-required", R_OK) == 0);
    e["required"] = required ? "true" : "false";

    std::string const reboot_date_filename(plugins()->get_server<sitter::server>()->get_cache_path("reboot.txt"));
    if(required)
    {
        time_t now(time(nullptr));
        time_t reboot_date(now);
        snapdev::file_contents reboot_date_file(reboot_date_filename);
        if(reboot_date_file.read_all())
        {
            std::string const & reboot_date_str(reboot_date_file.contents());
            std::int64_t date(0);
            if(advgetopt::validator_integer::convert_string(reboot_date_str, date))
            {
                reboot_date = date;
            }
        }
        else
        {
            reboot_date_file.contents(std::to_string(reboot_date));
            if(!reboot_date_file.write_all())
            {
                SNAP_LOG_ERROR
                    << "could not write to \""
                    << reboot_date_file.filename()
                    << "\" to save the reboot date."
                    << SNAP_LOG_SEND;
            }
        }

        // convert to days and compute the difference
        // (how many days has it been?)
        //
        now /= 86400;
        reboot_date /= 86400;
        time_t const diff(now - reboot_date);

        // TODO: offer the administrator to change the levels
        //       (see sitter::get_server_parameter() to retrieve values)
        //
        int priority(100);
        if(diff < 4)
        {
            priority = 45;
        }
        else if(diff < 10)
        {
            priority = 70;
        }
        else if(diff < 30)
        {
            priority = 90;
        }

        plugins()->get_server<sitter::server>()->append_error(
              json
            , "reboot"
            , "Reboot is required."
            , priority);
    }
    else
    {
        unlink(reboot_date_filename.c_str());
    }
}



} // namespace reboot
} // namespace sitter
// vim: ts=4 sw=4 et
