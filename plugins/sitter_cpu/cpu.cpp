// Snap Websites Server -- CPU watchdog: record CPU usage over time
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
#include    "cpu.h"


// libexcept
//
#include    <libexcept/exception.h>


// snaplogger
//
#include    <snaplogger/message.h>


// snapdev
//
#include    <snapdev/not_reached.h>
#include    <snapdev/not_used.h>


// C
//
#include    <proc/sysinfo.h>


// last include
//
#include    <snapdev/poison.h>



namespace sitter
{
namespace cpu
{

CPPTHREAD_PLUGIN_START(cpu, 1, 0)
    , ::cppthread::plugin_description(
            "Check the CPU load and instant usage.")
    , ::cppthread::plugin_dependency("server")
    , ::cppthread::plugin_help_uri("https://snapwebsites.org/help")
    , ::cppthread::plugin_categorization_tag("packages")
CPPTHREAD_PLUGIN_END(cpu)






/** \brief Initialize cpu.
 *
 * This function terminates the initialization of the cpu plugin
 * by registering for different events.
 *
 * \param[in] s  The child handling this request.
 */
void cpu::bootstrap(void * s)
{
    f_server = static_cast<server *>(s);

    CPPTHREAD_PLUGIN_LISTEN(cpu, "server", server, process_watch, boost::placeholders::_1);
}


/** \brief Process this watchdog data.
 *
 * This function runs this watchdog.
 *
 * \param[in] doc  The document.
 */
void cpu::on_process_watch(as2js::JSON::JSONValueRef & json)
{
    SNAP_LOG_DEBUG
        << "cpu::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    as2js::JSON::JSONValueRef e(json["cpu"]);

    // automatically initialized when loading the procps library
    e["count"] = smp_num_cpus;
    e["freq"] = Hertz;

    // total uptime and total idle time since boot
    {
        double uptime_secs(0.0);
        double idle_secs(0.0);
        uptime(&uptime_secs, &idle_secs); // also returns uptime_secs
        e["uptime"] = uptime_secs;
        e["idle"] = idle_secs;
    }

    // average CPU usage in the last 1 minute, 5 minutes, 15 minutes
    {
        double avg1(0.0);
        double avg5(0.0);
        double avg15(0.0);
        loadavg(&avg1, &avg5, &avg15);
        e.setAttribute("avg1", QString("%1").arg(avg1));
        e.setAttribute("avg5", QString("%1").arg(avg5));
        e.setAttribute("avg15", QString("%1").arg(avg15));
        e["avg1"] = avg1;
        e["avg5"] = avg5;
        e["avg15"] = avg15;

        // we always need the path to this cache file, if the CPU is
        // okay (not overloaded) then we want to delete the file
        // which in effect resets the timer
        //
        //QString cache_path(f_server->get_server_parameter(snap::watchdog::get_name(snap::watchdog::name_t::SNAP_NAME_WATCHDOG_CACHE_PATH)));
        std::string cache_path = "/var/cache/sitter";
        std::string const high_cpu_usage_filename(cache_path + "/high_cpu_usage.txt");

        double max_avg1(smp_num_cpus);
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
        if(static_cast<double>(avg1) >= max_avg1)
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
                        e["error"] = "High CPU usage";
                        f_server->append_error(
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

    // some additional statistics
    // (maybe one day they'll use a structure instead!)
    // typedef unsigned long jiff;
    // total CPU usage is in Jiffies (USER_HZ)
    {
        jiff cuse;                      // total CPU usage by user, normal processes
        jiff cice;                      // total CPU usage by user, niced processes
        jiff csys;                      // total CPU usage by system (kernel)
        jiff cide;                      // total CPU IDLE
        jiff ciow;                      // total CPU I/O wait
        jiff cxxx;                      // total CPU usage servicing interrupts
        jiff cyyy;                      // total CPU usage servicing softirqs
        jiff czzz;                      // total CPU usage running virtual hosts (steal time)
        unsigned long pin;              // page in (read back from cache)
        unsigned long pout;             // page out (freed from memory)
        unsigned long s_in;             // swap in (read back from swap)
        unsigned long sout;             // swap out (written to swap to free form memory)
        unsigned intr;                  // total number of interrupts so far
        unsigned ctxt;                  // total number of context switches
        unsigned int running;           // total number of processes currently running
        unsigned int blocked;           // total number of processes currently blcoked
        unsigned int btime;             // boot time (as a Unix time)
        unsigned int processes;         // total number of processes ever created

        getstat(&cuse, &cice, &csys, &cide, &ciow, &cxxx, &cyyy, &czzz,
             &pin, &pout, &s_in, &sout, &intr, &ctxt, &running, &blocked,
             &btime, &processes);

        e["total_cpu_user"] = cuse + cice;
        e["total_cpu_system"] = csys;
        e["total_cpu_wait"] = cide + ciow;
        e["page_cache_in"] = pin;
        e["page_cache_out"] = pout;
        e["swap_cache_in"] = s_in;
        e["swap_cache_out"] = sout;
        e["time_of_boot"] = btime;
        if(running > 1)
        {
            e["processes_running"] = running;
        }
        if(blocked != 0)
        {
            e["processes_blocked"] = blocked;
        }
        e["total_processes"] = processes;
    }
}



} // namespace apt
} // namespace sitter
// vim: ts=4 sw=4 et
