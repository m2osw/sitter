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
#include    "cpu.h"


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
namespace cpu
{

SERVERPLUGINS_START(cpu, 1, 0)
    , ::serverplugins::description(
            "Check the CPU load and instant usage.")
    , ::serverplugins::dependency("server")
    , ::serverplugins::help_uri("https://snapwebsites.org/help")
    , ::serverplugins::categorization_tag("packages")
SERVERPLUGINS_END(cpu)






/** \brief Initialize cpu.
 *
 * This function terminates the initialization of the cpu plugin
 * by registering for different events.
 */
void cpu::bootstrap()
{
    SERVERPLUGINS_LISTEN(cpu, "server", server, process_watch, boost::placeholders::_1);
}


/** \brief Process this sitter data.
 *
 * This function runs this plugin actual check.
 *
 * \param[in] json  The document where the results are collected.
 */
void cpu::on_process_watch(as2js::json::json_value_ref & json)
{
    SNAP_LOG_DEBUG
        << "cpu::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    as2js::json::json_value_ref e(json["cpu"]);

    sys_stats info;

    // automatically initialized when loading the procps library
    int const cpu_count(std::max(1U, std::thread::hardware_concurrency()));
    e["count"] = cpu_count;
    e["freq"] = sysconf(_SC_CLK_TCK);

    // total uptime and total idle time since boot
    e["uptime"] = info.get_uptime();
    e["idle"] = info.get_idle();

    // average CPU usage in the last 1 minute, 5 minutes, 15 minutes
    {
        e["avg1"] = info.get_load_avg1m();
        e["avg5"] = info.get_load_avg5m();
        e["avg15"] = info.get_load_avg15m();

        // we always need the path to this cache file, if the CPU is
        // okay (not overloaded) then we want to delete the file
        // which in effect resets the timer
        //
        std::string cache_path = "/var/cache/sitter";
        std::string const high_cpu_usage_filename(cache_path + "/high_cpu_usage.txt");

        double max_avg1(cpu_count);
        if(max_avg1 > 1.0) // with 1 CPU, go up to 100%
        {
            if(max_avg1 <= 2.0)
            {
                max_avg1 *= 0.95; // with 2 CPUs, go up to 95%
            }
            else
            {
                max_avg1 *= 0.8; // with 3+, go up to 80%
            }
        }

        if(info.get_load_avg1m() >= max_avg1)
        {
            // using too much of the CPUs is considered a warning, however,
            // if it lasts for too long (15 min.) it becomes an error
            //
            bool add_warning(true);

            time_t const start_date(time(nullptr));

            // to track the CPU over time, we create a file
            //
            // we use the cache location since the file should not stay
            // around for very long (a few minutes) or there is a problem
            // on the computer
            //
            snapdev::file_contents cpu_usage(high_cpu_usage_filename);
            if(cpu_usage.read_all())
            {
                std::string const & high_cpu_start_date_str(cpu_usage.contents());
                int64_t high_cpu_start_date(0);
                if(advgetopt::validator_integer::convert_string(high_cpu_start_date_str, high_cpu_start_date))
                {
                    // high CPU started more than 15 min. ago?
                    //
                    if(start_date - high_cpu_start_date > 15LL * 60LL)
                    {
                        // processors are overloaded on this machine
                        //
                        plugins()->get_server<sitter::server>()->append_error(
                              json
                            , "cpu"
                            , "High CPU usage."
                            , 100);
                        add_warning = false;
                    }
                }
            }
            else
            {
                cpu_usage.contents(std::to_string(start_date));
                if(!cpu_usage.write_all())
                {
                    SNAP_LOG_ERROR
                        << "could not write to \""
                        << cpu_usage.filename()
                        << "\" to save the start date."
                        << SNAP_LOG_SEND;
                }
            }

            if(add_warning)
            {
                e["warning"] = "High CPU usage";
            }
        }
        else
        {
            // CPU usage is not that high right now, eliminate the file
            //
            unlink(high_cpu_usage_filename.c_str());
        }
    }

    // CPU management
    //
    e["total_cpu_user"] = info.get_cpu_stat(cpu_t::CPU_USER_TIME) + info.get_cpu_stat(cpu_t::CPU_NICE_TIME);
    e["total_cpu_system"] = info.get_cpu_stat(cpu_t::CPU_SYSTEM_TIME);
    e["total_cpu_wait"] = info.get_cpu_stat(cpu_t::CPU_IDLE_TIME) + info.get_cpu_stat(cpu_t::CPU_IOWAIT_TIME);
    e["time_of_boot"] = info.get_boot_time();

    // process management
    //
    e["total_processes"] = info.get_processes();
    if(info.get_procs_running() > 1)
    {
        e["processes_running"] = info.get_procs_running();
    }
    if(info.get_procs_blocked() != 0)
    {
        e["processes_blocked"] = info.get_procs_blocked();
    }

    // memory management
    //
    e["page_cache_in"] = info.get_page_in();
    e["page_cache_out"] = info.get_page_out();
    e["swap_cache_in"] = info.get_page_swap_in();
    e["swap_cache_out"] = info.get_page_swap_out();
}



} // namespace cpu
} // namespace sitter
// vim: ts=4 sw=4 et
