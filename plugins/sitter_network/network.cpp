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
#include    "network.h"


// cppprocess
//
#include    <cppprocess/process_list.h>


// snaplogger
//
#include    <snaplogger/message.h>


// snapdev
//
#include    <snapdev/not_used.h>
#include    <snapdev/not_reached.h>


// serverplugins
//
#include    <serverplugins/collection.h>


// last include
//
#include    <snapdev/poison.h>





namespace sitter
{
namespace network
{


SERVERPLUGINS_START(network, 1, 0)
    , ::serverplugins::description(
            "Check that the network is up and running.")
    , ::serverplugins::dependency("server")
    , ::serverplugins::help_uri("https://snapwebsites.org/help")
    , ::serverplugins::categorization_tag("network")
SERVERPLUGINS_END(network)





/** \brief Initialize network.
 *
 * This function terminates the initialization of the network plugin
 * by registering for different events.
 */
void network::bootstrap()
{
    SERVERPLUGINS_LISTEN(network, "server", server, process_watch, boost::placeholders::_1);
}


/** \brief Process this sitter data.
 *
 * This function runs this plugin actual check.
 *
 * \param[in] json  The document where the results are collected.
 */
void network::on_process_watch(as2js::JSON::JSONValueRef & json)
{
    SNAP_LOG_DEBUG
        << "network::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    as2js::JSON::JSONValueRef results(json["network"]);
    if(find_communicatord(results))
    {
        // communicatord is running, it should have been giving us
        // some information such as how many neighbors it is connected
        // with
        //

        // TODO: implement
        if(verify_communicatord_connection(results))
        {
        }
        else
        {
        }
    }
    else
    {
        // add tests for when communicatord is not running
        // (maybe these only run after 5 min. to give other daemons
        // time to start)
        //

        // TODO: implement
    }
}


bool network::find_communicatord(as2js::JSON::JSONValueRef & json)
{
    cppprocess::process_list list;
    cppprocess::process_info::pointer_t info(list.find("communicatord"));

    // TODO: check whether the service is disabled if the output_process()
    //       function returns false; but really for communicatord that
    //       will be an EXTRA ERROR...

    return plugins()->get_server<sitter::server>()->output_process(
                    "network", json, info, "communicatord", 99);
}



bool network::verify_communicatord_connection(as2js::JSON::JSONValueRef & json)
{
    sitter::server::pointer_t server(plugins()->get_server<sitter::server>());
    if(!server->get_communicatord_is_connected())
    {
        // no communicatord process!?
        //
        as2js::JSON::JSONValueRef service(json["service"]);
        service["name"] = "communicatord";
        service["error"] = "not connected";

        time_t const connected(server->get_communicatord_connected_on());
        time_t const disconnected(server->get_communicatord_disconnected_on());
        time_t const now(time(nullptr));

        // amount of time since the last connection
        //
        time_t duration(now - disconnected);
        if(connected == 0)
        {
            // on startup, the process was never connected give the system
            // 5 min. to get started
            //
            duration = now - disconnected;
            if(duration < 5 * 60)
            {
                // don't report the error in this case
                //
                return false;
            }

            // to determine the priority from duration we remove the
            // first 5 min.
            //
            duration -= 5 * 60;
        }

        // depending on how long the connection has been missing, the
        // priority increases
        //
        int priority(15);
        if(duration > 15 * 60LL * 1000000LL) // 15 min.
        {
            priority = 100;
        }
        else if(duration > 5 * 60LL * 1000000LL) // 5 min.
        {
            priority = 65;
        }
        else if(duration > 60LL * 1000000LL) // 1 min.
        {
            priority = 30;
        }
        // else use default of 15

        server->append_error(
                  json
                , "network"
                , "found the \"communicatord\" process but somehow sitter is not connected, has not been for "
                    + std::to_string(duration)
                    + " seconds."
                , priority);

        return false;
    }

    // process running & we're connected!
    //
    return true;
}



} // namespace network
} // namespace sitter
// vim: ts=4 sw=4 et
