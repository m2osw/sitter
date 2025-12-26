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

// self
//
#include    <sitter/interrupt.h>
#include    <sitter/messenger.h>
#include    <sitter/sitter_worker.h>
#include    <sitter/tick_timer.h>


// eventdispatcher
//
#include    <eventdispatcher/communicator.h>


// serverplugins
//
#include    <serverplugins/server.h>
#include    <serverplugins/signals.h>


// cppprocess
//
#include    <cppprocess/process_list.h>


// cppthreadd
//
#include    <cppthread/thread.h>




namespace sitter
{


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
class server
    : public std::enable_shared_from_this<server>
    , public ed::connection_with_send_message
    , public ed::dispatcher
    , public serverplugins::server
{
public:
    typedef std::shared_ptr<server>         pointer_t;

    static constexpr std::int64_t const     MINIMUM_STATISTICS_FREQUENCY           = 60;      // 1 minute
    static constexpr std::int64_t const     DEFAULT_STATISTICS_FREQUENCY           = 60;      // 1 minute
    static constexpr std::int64_t const     MINIMUM_STATISTICS_PERIOD              = 3600;    // 1 hour
    static constexpr std::int64_t const     DEFAULT_STATISTICS_PERIOD              = 604800;  // 1 week
    static constexpr std::int64_t const     ROUND_STATISTICS_PERIOD                = 3600;    // round up to 1h
    static constexpr std::int64_t const     DEFAULT_STATISTICS_TTL                 = 604800;  // 1 week
    static constexpr std::int64_t const     MINIMUM_STATISTICS_TTL                 = 3600;    // 1 hour
    static constexpr std::int64_t const     DEFAULT_ERROR_REPORT_SETTLE_TIME       = 300;     // 5 minutes
    static constexpr std::int64_t const     MINIMUM_ERROR_REPORT_SETTLE_TIME       = 60;      // 1 minute
    static constexpr std::int64_t const     DEFAULT_ERROR_REPORT_LOW_PRIORITY      = 10;
    static constexpr std::int64_t const     MINIMUM_ERROR_REPORT_LOW_PRIORITY      = 1;
    static constexpr std::int64_t const     MAXIMUM_ERROR_REPORT_LOW_PRIORITY      = 50;
    static constexpr std::int64_t const     DEFAULT_ERROR_REPORT_LOW_SPAN          = 604800;  // 1 week
    static constexpr std::int64_t const     MINIMUM_ERROR_REPORT_LOW_SPAN          = 86400;   // 1 day
    static constexpr std::int64_t const     DEFAULT_ERROR_REPORT_MEDIUM_PRIORITY   = 50;
    static constexpr std::int64_t const     MINIMUM_ERROR_REPORT_MEDIUM_PRIORITY   = 10;
    static constexpr std::int64_t const     MAXIMUM_ERROR_REPORT_MEDIUM_PRIORITY   = 90;
    static constexpr std::int64_t const     DEFAULT_ERROR_REPORT_MEDIUM_SPAN       = 259200;  // 3 days
    static constexpr std::int64_t const     MINIMUM_ERROR_REPORT_MEDIUM_SPAN       = 3600;    // 1 hour
    static constexpr std::int64_t const     DEFAULT_ERROR_REPORT_CRITICAL_PRIORITY = 90;
    static constexpr std::int64_t const     MINIMUM_ERROR_REPORT_CRITICAL_PRIORITY = 1;
    static constexpr std::int64_t const     MAXIMUM_ERROR_REPORT_CRITICAL_PRIORITY = 100;
    static constexpr std::int64_t const     DEFAULT_ERROR_REPORT_CRITICAL_SPAN     = 86400;   // 1 day
    static constexpr std::int64_t const     MINIMUM_ERROR_REPORT_CRITICAL_SPAN     = 300;     // 5 minutes

                        server(int argc, char * argv[]);

    static void         set_instance(pointer_t s);
    static pointer_t    instance();
    int                 run();

    void                ready(ed::message & message);
    void                fluid_ready();
    void                stop(bool quitting);

    void                set_communicatord_connected(bool status);
    void                set_communicatord_disconnected(bool status);
    bool                get_communicatord_is_connected() const;
    snapdev::timespec_ex
                        get_communicatord_connected_on() const;
    snapdev::timespec_ex
                        get_communicatord_disconnected_on() const;
    std::string         get_cache_path(std::string const & filename);
    std::string         get_server_parameter(std::string const & name) const;

    PLUGIN_SIGNAL_WITH_MODE(process_watch, (as2js::json::json_value_ref & json), (json), NEITHER);

    // connection_with_send_message overloads
    //
    virtual bool        send_message(ed::message & message, bool cache = false) override;

    // internal functions (these are NOT virtual)
    // 
    void                process_tick();

    void                msg_rusage(ed::message & message);
    void                msg_reload_config(ed::message & message);

    void                clear_cache(std::string const & name);
    bool                output_process(
                              std::string const & plugin_name
                            , as2js::json::json_value_ref & json
                            , cppprocess::process_info::pointer_t info
                            , std::string const & process_name
                            , int priority);

    void                clear_errors();
    void                append_error(
                              as2js::json::json_value_ref & json_ref
                            , std::string const & plugin_name
                            , std::string const & message
                            , int priority = 50);
    int                 get_error_count() const;
    int                 get_max_error_priority() const;

    std::int64_t        get_statistics_frequency();
    std::int64_t        get_statistics_period();
    std::int64_t        get_statistics_ttl();
    std::int64_t        get_error_report_settle_time();
    std::int64_t        get_error_report_low_priority();
    std::int64_t        get_error_report_low_span();
    std::int64_t        get_error_report_medium_priority();
    std::int64_t        get_error_report_medium_span();
    std::int64_t        get_error_report_critical_priority();
    std::int64_t        get_error_report_critical_span();

    void                set_ticks(int ticks);
    int                 get_ticks() const;

private:
    void                define_server_name();
    void                record_usage(ed::message const & message);

    advgetopt::getopt   f_opts;
    ed::communicator::pointer_t
                        f_communicator = ed::communicator::pointer_t();
    interrupt::pointer_t
                        f_interrupt = interrupt::pointer_t();
    tick_timer::pointer_t
                        f_tick_timer = tick_timer::pointer_t();
    messenger::pointer_t
                        f_messenger = messenger::pointer_t();

    std::int64_t        f_statistics_frequency = -1;
    std::int64_t        f_statistics_period = -1;
    std::int64_t        f_statistics_ttl = -1;
    std::int64_t        f_error_report_settle_time = -1;
    std::int64_t        f_error_report_low_priority = -1;
    std::int64_t        f_error_report_low_span = -1;
    std::int64_t        f_error_report_medium_priority = -1;
    std::int64_t        f_error_report_medium_span = -1;
    std::int64_t        f_error_report_critical_priority = -1;
    std::int64_t        f_error_report_critical_span = -1;
    int                 f_error_count = 0;
    int                 f_max_error_priority = 0;
    bool                f_stopping = false;
    bool                f_force_restart = false;
    snapdev::timespec_ex
                        f_communicatord_connected = 0.0;
    snapdev::timespec_ex
                        f_communicatord_disconnected = 0.0;
    std::string         f_cache_path = std::string();
    int                 f_ticks = 0;

    worker_done::pointer_t
                        f_worker_done = worker_done::pointer_t();
    sitter_worker::pointer_t
                        f_worker = sitter_worker::pointer_t();
    cppthread::thread::pointer_t
                        f_worker_thread = cppthread::thread::pointer_t();
};
#pragma GCC diagnostic pop


} // namespace sitter
// vim: ts=4 sw=4 et
