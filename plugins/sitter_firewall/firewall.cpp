// Copyright (c) 2013-2023  Made to Order Software Corp.  All Rights Reserved.
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
#include    "firewall.h"


// cppprocess
//
#include    <cppprocess/process_list.h>


// snaplogger
//
#include    <snaplogger/message.h>


// snapdev
//
#include    <snapdev/not_reached.h>
#include    <snapdev/not_used.h>


// serverplugins
//
#include    <serverplugins/collection.h>


// last include
//
#include    <snapdev/poison.h>



namespace sitter
{
namespace firewall
{

SERVERPLUGINS_START(firewall)
    , ::serverplugins::description(
            "Check whether the Apache server is running.")
    , ::serverplugins::dependency("server")
    , ::serverplugins::help_uri("https://snapwebsites.org/help")
    , ::serverplugins::categorization_tag("security")
    , ::serverplugins::categorization_tag("firewall")
SERVERPLUGINS_END(firewall)





/** \brief Initialize firewall.
 *
 * This function terminates the initialization of the firewall plugin
 * by registering for various events.
 */
void firewall::bootstrap()
{
    SERVERPLUGINS_LISTEN(firewall, "server", server, process_watch, boost::placeholders::_1);
}


/** \brief Process this sitter data.
 *
 * This function runs this plugin actual check.
 *
 * \param[in] json  The document where the results are collected.
 */
void firewall::on_process_watch(as2js::json::json_value_ref & json)
{
    SNAP_LOG_DEBUG
        << "firewall::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    as2js::json::json_value_ref e(json["firewall"]);

    // first we check that the snapfirewall daemon is running
    //
    cppprocess::process_list list;
    cppprocess::process_info::pointer_t info(list.find("snapfirewall"));
    if(!plugins()->get_server<sitter::server>()->output_process("firewall", e, info, "snapfirewall", 95))
    {
        return;
    }

    // TODO:
    // check that certain rules exist, that way we make sure that it's
    // really up (although we do not really have a good way to test
    // the system other than that...)
    //
    // long term it would be great to have a way to do a kind of nmap
    // to see what ports are open on what IP, that way we can make sure
    // that only ports that we generally allow are opened and all the
    // others are not, however, an nmap test is very long and slow so
    // it would required another tool we run once a day with a report
    // of the current firewall status
    //
}



} // namespace firewall
} // namespace sitter
// vim: ts=4 sw=4 et
