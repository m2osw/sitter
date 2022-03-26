// Snap Websites Server -- Network watchdog
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


// last include
//
#include    <snapdev/poison.h>





namespace sitter
{
namespace network
{


CPPTHREAD_PLUGIN_START(network, 1, 0)
    , ::cppthread::plugin_description(
            "Check that the network is up and running.")
    , ::cppthread::plugin_dependency("server")
    , ::cppthread::plugin_help_uri("https://snapwebsites.org/help")
    , ::cppthread::plugin_categorization_tag("network")
CPPTHREAD_PLUGIN_END(network)





/** \brief Initialize network.
 *
 * This function terminates the initialization of the network plugin
 * by registering for different events.
 *
 * \param[in] s  The sitter server.
 */
void network::bootstrap(void * s)
{
    f_server = static_cast<server *>(s);

    CPPTHREAD_PLUGIN_LISTEN(network, "server", server, process_watch, boost::placeholders::_1);
}


/** \brief Process this watchdog data.
 *
 * This function runs this watchdog.
 *
 * \param[in] doc  The document.
 */
void network::on_process_watch(as2js::JSON::JSONValueRef & json)
{
    SNAP_LOG_DEBUG
        << "network::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    as2js::JSON::JSONValueRef results(json["network"]);
    if(find_snapcommunicator(results))
    {
        // snapcommunicator is running, it should have been giving us
        // some information such as how many neighbors it is connected
        // with
        //

        // TODO: implement
        if(verify_snapcommunicator_connection(results))
        {
        }
        else
        {
        }
    }
    else
    {
        // add tests for when snapcommunicator is not running
        // (maybe these only run after 5 min. to give other daemons
        // time to start)
        //

        // TODO: implement
    }
}


bool network::find_snapcommunicator(as2js::JSON::JSONValueRef & json)
{
    cppprocess::process_list list;
    cppprocess::process_info::pointer_t info(list.find("snapcommunicator"));

    // TODO: check whether the service is disabled if the output_process()
    //       function returns false; but really for snapcommunicator that
    //       will be an EXTRA ERROR...

    return f_sitter->output_process(json, info, "network");
}



bool network::verify_snapcommunicator_connection(as2js::JSON::JSONValueRef & json)
{
    if(!f_sitter->get_snapcommunicator_is_connected())
    {
        // no snapcommunicator process!?
        //
        as2js::JSON::JSONValueRef service(json["service"]);
        service["name"] = "snapcommunicator";
        service["error"] = "not connected";

        time_t const connected(server->get_snapcommunicator_connected_on());
        time_t const disconnected(server->get_snapcommunicator_disconnected_on());
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

        f_sitter->append_error(
                  json
                , "network"
                , "found the \"snapcommunicator\" process but somehow sitter is not connected, has not been for "
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
