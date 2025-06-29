// Copyright (c) 2011-2025  Made to Order Software Corp.  All Rights Reserved.
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
#include    "sitter/sys_stats.h"

#include    "sitter/exception.h"


// advgetopt
//
#include    <advgetopt/validator_double.h>
#include    <advgetopt/validator_integer.h>


// snapdev
//
#include    <snapdev/file_contents.h>


// last include
//
#include    <snapdev/poison.h>





/** \file
 * \brief This file implements the system statistics gathering.
 *
 * One of the jobs of the sitter is to gather system statistics to have an
 * idea of its usage. This is then shared with all the other sitters on
 * all the other computers through the communicatord service.
 *
 * It is made available in the sitter library so others can also gather
 * the system settings as required.
 */



namespace sitter
{


void sys_stats::reset()
{
    f_defined.clear();
}


double sys_stats::get_uptime()
{
    load_uptime();
    return f_uptime;
}


double sys_stats::get_idle()
{
    load_uptime();
    return f_idle;
}


double sys_stats::get_load_avg1m()
{
    load_loadavg();
    return f_avg[static_cast<int>(loadavg_t::LOADAVG_1MIN)];
}


double sys_stats::get_load_avg5m()
{
    load_loadavg();
    return f_avg[static_cast<int>(loadavg_t::LOADAVG_5MIN)];
}


double sys_stats::get_load_avg15m()
{
    load_loadavg();
    return f_avg[static_cast<int>(loadavg_t::LOADAVG_15MIN)];
}


std::int64_t sys_stats::get_running_threads()
{
    load_loadavg();
    return f_running_threads;
}


std::int64_t sys_stats::get_total_threads()
{
    load_loadavg();
    return f_total_threads;
}


pid_t sys_stats::get_last_created_process()
{
    load_loadavg();
    return f_last_created_process;
}


std::int64_t sys_stats::get_cpu_stat(cpu_t field)
{
    if(static_cast<int>(field) < 0
    || static_cast<int>(field) >= static_cast<int>(cpu_t::CPU_max))
    {
        throw out_of_range("CPU field out of range");
    }

    load_stat();
    return f_cpu[static_cast<int>(field)];
}


std::int64_t sys_stats::get_intr()
{
    load_stat();
    return f_intr;
}


std::int64_t sys_stats::get_ctxt()
{
    load_stat();
    return f_ctxt;
}


time_t sys_stats::get_boot_time()
{
    load_stat();
    return f_boot_time;
}


std::int64_t sys_stats::get_processes()
{
    load_stat();
    return f_processes;
}


std::int64_t sys_stats::get_procs_running()
{
    load_stat();
    return f_procs_running;
}


std::int64_t sys_stats::get_procs_blocked()
{
    load_stat();
    return f_procs_blocked;
}


std::int64_t sys_stats::get_page_in()
{
    load_vmstats();
    return get_map_int64(f_vmstats, "pgpgin");
}


std::int64_t sys_stats::get_page_out()
{
    load_vmstats();
    return get_map_int64(f_vmstats, "pgpgout");
}


std::int64_t sys_stats::get_page_swap_in()
{
    load_vmstats();
    return get_map_int64(f_vmstats, "pswpin");
}


std::int64_t sys_stats::get_page_swap_out()
{
    load_vmstats();
    return get_map_int64(f_vmstats, "pswpout");
}


void sys_stats::load_uptime()
{
    auto const d(f_defined.insert(defined_t::DEFINED_UPTIME));
    if(!d.second)
    {
        return;
    }

    snapdev::file_contents in("/proc/uptime");
    if(in.read_all())
    {
        std::string const uptime_idle(in.contents());
        advgetopt::string_list_t values;
        advgetopt::split_string(uptime_idle, values, {" "});
        if(values.size() >= 1)
        {
            advgetopt::validator_double::convert_string(values[0], f_uptime);
        }
        if(values.size() >= 2)
        {
            advgetopt::validator_double::convert_string(values[1], f_idle);
        }
    }
}


void sys_stats::load_loadavg()
{
    auto const d(f_defined.insert(defined_t::DEFINED_LOADAVG));
    if(!d.second)
    {
        return;
    }

    snapdev::file_contents in("/proc/loadavg");
    if(in.read_all())
    {
        std::string const uptime_idle(in.contents());
        advgetopt::string_list_t values;
        advgetopt::split_string(uptime_idle, values, {" "});
        for(std::size_t idx(0); idx < 3 && idx < values.size(); ++idx)
        {
            advgetopt::validator_double::convert_string(values[idx], f_avg[idx]);
        }
        if(values.size() >= 4)
        {
            advgetopt::string_list_t running_threads;
            advgetopt::split_string(values[3], running_threads, {"/"});
            if(values.size() >= 1)
            {
                advgetopt::validator_integer::convert_string(running_threads[0], f_running_threads);
            }
            if(values.size() >= 2)
            {
                advgetopt::validator_integer::convert_string(running_threads[1], f_total_threads);
            }
        }
        if(values.size() >= 5)
        {
            advgetopt::validator_integer::convert_string(values[4], f_last_created_process);
        }
    }
}

void sys_stats::load_stat()
{
    auto const d(f_defined.insert(defined_t::DEFINED_STATS));
    if(!d.second)
    {
        return;
    }

    snapdev::file_contents in("/proc/stat");
    if(in.read_all())
    {
        std::string const stats(in.contents());
        advgetopt::string_list_t lines;
        advgetopt::split_string(stats, lines, {"\n"});
        for(auto const & l : lines)
        {
            if(l.empty())
            {
                continue;
            }
            switch(l[0])
            {
            case 'b':
                if(l.length() > 6
                && l[1] == 't'
                && l[2] == 'i'
                && l[3] == 'm'
                && l[4] == 'e'
                && l[5] == ' ')     // btime ...
                {
                    advgetopt::string_list_t values;
                    advgetopt::split_string(l, values, {" "});
                    if(values.size() >= 2)
                    {
                        std::int64_t boot_time(0);
                        advgetopt::validator_integer::convert_string(values[1], boot_time);
                        f_boot_time = boot_time;
                    }
                }
                break;

            case 'c':
                if(l.length() > 4
                && l[1] == 'p'
                && l[2] == 'u'
                && l[3] == ' ')     // cpu ...
                {
                    // TODO: support reading all CPUs
                    //
                    advgetopt::string_list_t values;
                    advgetopt::split_string(l, values, {" "});
                    for(int idx(0); idx < static_cast<int>(cpu_t::CPU_max); ++idx)
                    {
                        advgetopt::validator_integer::convert_string(values[idx + 1], f_cpu[idx]);
                    }
                }
                else if(l.length() > 5
                     && l[1] == 't'
                     && l[2] == 'x'
                     && l[3] == 't'
                     && l[4] == ' ')    // ctxt ...
                {
                    advgetopt::string_list_t values;
                    advgetopt::split_string(l, values, {" "});
                    if(values.size() >= 2)
                    {
                        // only get total number
                        advgetopt::validator_integer::convert_string(values[1], f_ctxt);
                    }
                }
                break;

            case 'i':
                if(l.length() > 5
                && l[1] == 'n'
                && l[2] == 't'
                && l[3] == 'r'
                && l[4] == ' ')  // intr ...
                {
                    advgetopt::string_list_t values;
                    advgetopt::split_string(l, values, {" "});
                    if(values.size() >= 2)
                    {
                        // only get total number
                        advgetopt::validator_integer::convert_string(values[1], f_intr);
                    }
                }
                break;

            case 'p':
                if(l.length() > 11      // starts with "proc"...
                && l[1] == 'p'
                && l[2] == 'r'
                && l[3] == 'o'
                && l[4] == 'c')
                {
                    if(l[5] == 'e'
                    && l[6] == 's'
                    && l[7] == 's'
                    && l[8] == 'e'
                    && l[9] == 's')
                    {
                        advgetopt::string_list_t values;
                        advgetopt::split_string(l, values, {" "});
                        if(values.size() >= 2)
                        {
                            advgetopt::validator_integer::convert_string(values[1], f_processes);
                        }
                    }
                    else if(l.length() > 15      // continues with "[proc]s_"...
                    && l[5] == 's'
                    && l[6] == '_')
                    {
                        if(l[7] == 'r'
                        && l[8] == 'u'
                        && l[9] == 'n'
                        && l[10] == 'n'
                        && l[11] == 'i'
                        && l[12] == 'n'
                        && l[13] == 'g'
                        && l[14] == ' ')        // procs_running ...
                        {
                            advgetopt::string_list_t values;
                            advgetopt::split_string(l, values, {" "});
                            if(values.size() >= 2)
                            {
                                advgetopt::validator_integer::convert_string(values[1], f_procs_running);
                            }
                        }
                        else if(l[7] == 'b'
                             && l[8] == 'l'
                             && l[9] == 'o'
                             && l[10] == 'c'
                             && l[11] == 'k'
                             && l[12] == 'e'
                             && l[13] == 'd'
                             && l[14] == ' ')
                        {
                            advgetopt::string_list_t values;
                            advgetopt::split_string(l, values, {" "});
                            if(values.size() >= 2)
                            {
                                advgetopt::validator_integer::convert_string(values[1], f_procs_blocked);
                            }
                        }
                    }
                }
                break;

            }
        }
    }
}


void sys_stats::load_vmstats()
{
    load_map("/proc/vmstat", defined_t::DEFINED_VMSTATS, f_vmstats);
}


void sys_stats::load_map(
      std::string const & filename
    , defined_t defined
    , data_map_t & out)
{
    out.clear();

    auto const d(f_defined.insert(defined));
    if(!d.second)
    {
        return;
    }

    snapdev::file_contents in(filename);
    if(!in.read_all())
    {
        return;
    }
    std::string const & stats(in.contents());

    advgetopt::string_list_t lines;
    advgetopt::split_string(stats, lines, {"\n"});
    for(auto const & l : lines)
    {
        std::string::size_type const pos(l.find(' '));
        if(pos == std::string::npos)
        {
            continue;
        }

        out[l.substr(0, pos)] = l.substr(pos + 1);
    }
}


std::int64_t sys_stats::get_map_int64(
      data_map_t & m
    , std::string const & name)
{
    auto it(m.find(name));
    if(it == m.end())
    {
        return 0;
    }

    std::int64_t result(0);
    advgetopt::validator_integer::convert_string(it->second, result);
    return result;
}



} // namespace sitter
// vim: ts=4 sw=4 et
