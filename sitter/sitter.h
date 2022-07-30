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
#pragma once

// self
//
#include    <sitter/sitter_worker.h>


// eventdispatcher
//
#include    <eventdispatcher/dispatcher.h>
#include    <eventdispatcher/logrotate_udp_messenger.h>


// advgetopt
//
#include    <advgetopt/options.h>


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


// as2js
//
#include    <as2js/json.h>



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

                        server(int argc, char * argv[]);

    static void         set_instance(pointer_t s);
    static pointer_t    instance();
    int                 run();

    int64_t             get_statistics_period() const { return f_statistics_period; }
    int64_t             get_statistics_ttl() const { return f_statistics_ttl; }
    void                ready(ed::message & message);
    void                stop(bool quitting);
    void                set_communicatord_connected(bool status);
    void                set_communicatord_disconnected(bool status);
    bool                get_communicatord_is_connected() const;
    time_t              get_communicatord_connected_on() const;
    time_t              get_communicatord_disconnected_on() const;
    std::string         get_cache_path(std::string const & filename);
    std::string         get_server_parameter(std::string const & name) const;

    PLUGIN_SIGNAL_WITH_MODE(process_watch, (as2js::JSON::JSONValueRef & json), (json), NEITHER);

    // connection_with_send_message overloads
    //
    virtual bool        send_message(ed::message & message, bool cache = false) override;

    // internal functions (these are NOT virtual)
    // 
    void                process_tick();
    void                process_sigchld();

    //void                msg_nocassandra(ed::message & message);
    //void                msg_cassandraready(ed::message & message);
    void                msg_rusage(ed::message & message);
    void                msg_reload_config(ed::message & message);

    bool                output_process(
                              std::string const & plugin_name
                            , as2js::JSON::JSONValueRef & json
                            , cppprocess::process_info::pointer_t info
                            , std::string const & process_name
                            , int priority);

    void                clear_errors();
    void                append_error(
                              as2js::JSON::JSONValueRef & json_ref
                            , std::string const & plugin_name
                            , std::string const & message
                            , int priority = 50);
    int                 get_error_count() const;
    int                 get_max_error_priority() const;

    int64_t             get_error_report_settle_time() const;
    int64_t             get_error_report_low_priority() const;
    int64_t             get_error_report_low_span() const;
    int64_t             get_error_report_medium_priority() const;
    int64_t             get_error_report_medium_span() const;
    int64_t             get_error_report_critical_priority() const;
    int64_t             get_error_report_critical_span() const;

    void                set_ticks(int ticks);
    int                 get_ticks() const;

private:
    void                define_server_name();
    bool                init_parameters();
    void                record_usage(ed::message const & message);

    advgetopt::getopt   f_opts;
    ed::logrotate_extension
                        f_logrotate;
    int64_t             f_statistics_frequency = 0;
    int64_t             f_statistics_period = 0;
    int64_t             f_statistics_ttl = 0;
    int64_t             f_error_report_settle_time = 5 * 60;
    int64_t             f_error_report_low_priority = 10;
    int64_t             f_error_report_low_span = 86400 * 7;
    int64_t             f_error_report_medium_priority = 50;
    int64_t             f_error_report_medium_span = 86400 * 3;
    int64_t             f_error_report_critical_priority = 90;
    int64_t             f_error_report_critical_span = 86400 * 1;
    int                 f_error_count = 0;
    int                 f_max_error_priority = 0;
    bool                f_stopping = false;
    bool                f_force_restart = false;
    time_t              f_communicatord_connected = 0;
    time_t              f_communicatord_disconnected = 0;
    std::string         f_cache_path = std::string();
    int                 f_ticks = 0;

    sitter_worker::pointer_t
                        f_worker = sitter_worker::pointer_t();
    cppthread::thread::pointer_t
                        f_worker_thread = cppthread::thread::pointer_t();
};
#pragma GCC diagnostic pop


} // namespace sitter
// vim: ts=4 sw=4 et
