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
#pragma once

// C++
//
#include    <cstdint>
#include    <map>
#include    <memory>
#include    <numeric>
#include    <set>


// C
//
#include    <unistd.h>



namespace sitter
{



enum class cpu_t
{
    CPU_USER_TIME,
    CPU_NICE_TIME,
    CPU_SYSTEM_TIME,
    CPU_IDLE_TIME,
    CPU_IOWAIT_TIME,
    CPU_IRQ_TIME,
    CPU_SOFTIRQ_TIME,
    CPU_STEAL_TIME,
    CPU_GUEST_TIME,

    CPU_max
};


class sys_stats
{
public:
    typedef std::shared_ptr<sys_stats>  pointer_t;

    void                reset();

    double              get_uptime();
    double              get_idle();
    double              get_load_avg1m();
    double              get_load_avg5m();
    double              get_load_avg15m();
    std::int64_t        get_running_threads();
    std::int64_t        get_total_threads();
    pid_t               get_last_created_process();
    std::int64_t        get_cpu_stat(cpu_t field);
    std::int64_t        get_intr();
    std::int64_t        get_ctxt();
    time_t              get_boot_time();
    std::int64_t        get_processes();
    std::int64_t        get_procs_running();
    std::int64_t        get_procs_blocked();
    std::int64_t        get_page_in();
    std::int64_t        get_page_out();
    std::int64_t        get_page_swap_in();
    std::int64_t        get_page_swap_out();

private:
    typedef std::map<std::string, std::string>      data_map_t;

    enum class defined_t
    {
        DEFINED_UPTIME,
        DEFINED_LOADAVG,
        DEFINED_STATS,
        DEFINED_VMSTATS,
    };

    enum class loadavg_t
    {
        LOADAVG_1MIN,
        LOADAVG_5MIN,
        LOADAVG_15MIN,

        LOADAVG_max
    };

    void                load_uptime();
    void                load_loadavg();
    void                load_stat();
    void                load_map(
                              std::string const & filename
                            , defined_t defined
                            , data_map_t & out);
    void                load_vmstats();
    std::int64_t        get_map_int64(
                              data_map_t & out
                            , std::string const & name);

    std::set<defined_t> f_defined = std::set<defined_t>();

    double              f_uptime = 0.0;
    double              f_idle = 0.0;

    double              f_avg[static_cast<int>(loadavg_t::LOADAVG_max)] = {};
    std::int64_t        f_running_threads = 0;
    std::int64_t        f_total_threads = 0;
    std::int64_t        f_last_created_process = 0; // PID
    std::int64_t        f_cpu[static_cast<int>(cpu_t::CPU_max)];
    std::int64_t        f_intr = 0;
    std::int64_t        f_ctxt = 0;
    time_t              f_boot_time = 0;
    std::int64_t        f_processes = 0;
    std::int64_t        f_procs_running = 0;
    std::int64_t        f_procs_blocked = 0;
    data_map_t          f_vmstats = data_map_t();
};


} // namespace sitter
// vim: ts=4 sw=4 et
