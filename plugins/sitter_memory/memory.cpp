// Copyright (c) 2013-2025  Made to Order Software Corp.  All Rights Reserved
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
#include    "memory.h"


// sitter
//
#include    <sitter/meminfo.h>


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
namespace memory
{


SERVERPLUGINS_START(memory)
    , ::serverplugins::description(
            "Check current memory usage.")
    , ::serverplugins::dependency("server")
    , ::serverplugins::help_uri("https://snapwebsites.org/help")
    , ::serverplugins::categorization_tag("os")
SERVERPLUGINS_END(memory)




/** \brief Initialize memory.
 *
 * This function terminates the initialization of the memory plugin
 * by registering for different events.
 */
void memory::bootstrap()
{
    SERVERPLUGINS_LISTEN(memory, server, process_watch, boost::placeholders::_1);
}


/** \brief Process this sitter data.
 *
 * This function runs this plugin actual check.
 *
 * \param[in] json  The document where the results are collected.
 */
void memory::on_process_watch(as2js::json::json_value_ref & json)
{
    SNAP_LOG_DEBUG
        << "memory::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    as2js::json::json_value_ref e(json["memory"]);

    // read "/proc/meminfo"
    //
    sitter::meminfo_t const info(get_meminfo());

    // simple memory data should always be available
    e["mem_total"] =     info.f_mem_total;
    e["mem_free"] =      info.f_mem_free;
    e["mem_available"] = info.f_mem_available;
    e["mem_buffers"] =   info.f_buffers;
    e["mem_cached"] =    info.f_cached;
    e["swap_cached"] =   info.f_swap_cached;
    e["swap_total"] =    info.f_swap_total;
    e["swap_free"] =     info.f_swap_free;

    bool const high_memory_usage([info]()
            {
                // if we have at least 512MB don't generate an error
                //
                if(info.f_mem_available > 512.0 * 1024.0 * 1024.0)
                {
                    return false;
                }

                // otherwise see whether we use over 80% of the RAM
                //
                // NOTE: the 0.2 value is going to make it true immediatly
                //       if the total amount of memory is about 8Gb or more.
                //
                double const mem_left_percent(static_cast<double>(info.f_mem_available) / static_cast<double>(info.f_mem_total));
                return mem_left_percent < 0.2;
            }());
    if(high_memory_usage)
    {
        plugins()->get_server<sitter::server>()->append_error(
              e
            , "memory"
            , "High memory usage"
            , 75);
    }

    bool const high_swap_usage([info]()
            {
                // for swap, we generate errors once 50% is in use, it should
                // never be much more than 10% for a good system
                //
                double const swap_left_percent(static_cast<double>(info.f_swap_free) / static_cast<double>(info.f_swap_total));
                return swap_left_percent < 0.5;
            }());
    if(high_swap_usage)
    {
        plugins()->get_server<sitter::server>()->append_error(
              e
            , "memory"
            , "High swap usage"
            , 65);
    }

    // TODO: we should probably look into processing /proc/swaps
    //       as well, that way we would get details such as what
    //       files / partition is being used, etc.
}



} // namespace memory
} // namespace sitter
// vim: ts=4 sw=4 et
