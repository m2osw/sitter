// Copyright (c) 2011-2023  Made to Order Software Corp.  All Rights Reserved.
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
#include    "sitter/sitter.h"

#include    "sitter/exception.h"
#include    "sitter/names.h"
#include    "sitter/version.h"


// snaplogger
//
#include    <snaplogger/logger.h>
#include    <snaplogger/message.h>
#include    <snaplogger/options.h>


// advgetopt
//
#include    <advgetopt/conf_file.h>
#include    <advgetopt/exception.h>
#include    <advgetopt/validator_duration.h>
#include    <advgetopt/validator_integer.h>


// snapdev
//
#include    <snapdev/file_contents.h>
#include    <snapdev/gethostname.h>
#include    <snapdev/glob_to_list.h>
#include    <snapdev/mkdir_p.h>
#include    <snapdev/string_replace_many.h>


// last include
//
#include    <snapdev/poison.h>





/** \file
 * \brief This file represents the Sitter daemon.
 *
 * The sitter.cpp and sitter.h files represents the sitter daemon.
 *
 * This is not exactly a service, although it somewhat (mostly) behaves
 * like one. The sitter is used as a daemon to make sure that various
 * resources on a server remain available as expected.
 */



/** \mainpage
 * \brief Sitter Documentation
 *
 * \section introduction Introduction
 *
 * The Sitter is a tool that works in unisson with Snap! C++.
 *
 * It is used to monitor all the services used by Snap! C++ in order to
 * ensure that they all continuously work as expected.
 *
 * It also gathers system information such as how busy a server is so as
 * to allow proxying requests between front end servers to better distribute
 * load.
 */


namespace sitter
{


namespace
{


/** \brief The sitter server.
 *
 * This variable holds the server. The server::instance() function returns
 * the pointer. However, it does not allocate it. The main.cpp of the daemon
 * implementation allocates the server passing the argc/argv parameters and
 * then it saves it using the set_instance() function.
 *
 * At this point, this pointer never gets reset.
 *
 * \note
 * Having an instance() function is a requirement of the serverplugins
 * implementation.
 */
server::pointer_t               g_server;



advgetopt::option const g_options[] =
{
    advgetopt::end_options()
};


advgetopt::group_description const g_group_descriptions[] =
{
    advgetopt::define_group(
          advgetopt::GroupNumber(advgetopt::GETOPT_FLAG_GROUP_COMMANDS)
        , advgetopt::GroupName("command")
        , advgetopt::GroupDescription("Commands:")
    ),
    advgetopt::define_group(
          advgetopt::GroupNumber(advgetopt::GETOPT_FLAG_GROUP_OPTIONS)
        , advgetopt::GroupName("option")
        , advgetopt::GroupDescription("Options:")
    ),
    advgetopt::end_groups()
};


// until we have C++20 remove warnings this way
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
advgetopt::options_environment const g_options_environment =
{
    .f_project_name = "sitter",
    .f_group_name = nullptr,
    .f_options = g_options,
    .f_options_files_directory = nullptr,
    .f_environment_variable_name = "SITTER_OPTIONS",
    .f_environment_variable_intro = "SITTER_",
    .f_section_variables_name = nullptr,
    .f_configuration_files = nullptr,
    .f_configuration_filename = "sitter.conf",
    .f_configuration_directories = nullptr,
    .f_environment_flags = advgetopt::GETOPT_ENVIRONMENT_FLAG_SYSTEM_PARAMETERS
                         | advgetopt::GETOPT_ENVIRONMENT_FLAG_PROCESS_SYSTEM_PARAMETERS,
    .f_help_header = "Usage: %p [-<opt>] <process-name>\n"
                     "where -<opt> is one or more of:",
    .f_help_footer = "Additional command line options loaded from: %i\n\n%c",
    .f_version = SITTER_VERSION_STRING,
    .f_license = "GNU GPL v2",
    .f_copyright = "Copyright (c) 2013-"
                   BOOST_PP_STRINGIZE(UTC_BUILD_YEAR)
                   " by Made to Order Software Corporation -- All Rights Reserved",
    .f_build_date = UTC_BUILD_DATE,
    .f_build_time = UTC_BUILD_TIME,
    .f_groups = g_group_descriptions,
};
#pragma GCC diagnostic pop



} // no name namespace




/** \brief Initialize the sitter server.
 *
 * This constructor makes sure to setup the correct filename for the
 * sitter server configuration file.
 */
server::server(int argc, char * argv[])
    : dispatcher(this)
    , serverplugins::server(serverplugins::get_id("sitter"))
    , f_opts(g_options_environment)
    , f_communicator(ed::communicator::instance())
    , f_messenger(std::make_shared<messenger>(this, f_opts))
    , f_communicatord_disconnected(snapdev::timespec_ex::gettime())
{
    snaplogger::add_logger_options(f_opts);
    add_plugin_options();
    f_opts.finish_parsing(argc, argv);
    if(!snaplogger::process_logger_options(f_opts, "/etc/sitter/logger"))
    {
        // exit on any error
        //
        throw advgetopt::getopt_exit("logger options generated an error.", 0);
    }

    // further dispatcher initialization
    //
#ifdef _DEBUG
    set_trace();
    set_show_matches();
#endif

    add_matches({
        DISPATCHER_MATCH("RELOADCONFIG", &server::msg_reload_config),
        DISPATCHER_MATCH("RUSAGE",       &server::msg_rusage),
    });
}


/** \brief Save the pointer to the instance of the server.
 *
 * The server is created by the main() function. It then calls this function
 * to save the pointer_t of the server in a global variable managed internally
 * and accessible from the instance() function.
 *
 * \exception logic_error
 * If the g_server pointer is already set (not nullptr), then this exception
 * is raised.
 *
 * \param[in] s  The new server.
 */
void server::set_instance(pointer_t s)
{
    if(g_server != nullptr)
    {
        throw logic_error("the server is already defined.");
    }

    g_server = s;
}


/** \brief Retrieve a pointer to the sitter server.
 *
 * This function retrieve an instance pointer of the sitter server.
 * If the instance does not exist yet, then it gets created. A
 * server is also a plugin which is named "server".
 *
 * \exception logic_error
 * This exception is raised if this function gets called before the
 * set_instance() happens.
 *
 * \return The managed pointer to the sitter server.
 */
server::pointer_t server::instance()
{
    if(g_server == nullptr)
    {
        throw logic_error("the server pointer was not yet defined with set_instance().");
    }

    return g_server;
}


/** \brief Load the plugin options.
 *
 * The command line options are loaded from the plugin .ini files too so that
 * way each plugin can have its own set of options.
 */
void server::add_plugin_options()
{
    std::string name(f_opts.get_options_filename());
    if(name.length() < 5)
    {
        return;
    }

    if(name.substr(name.length() - 4) != ".ini")
    {
        SNAP_LOG_WARNING
            << "the options filename ("
            << name
            << ") does not end with \".ini\"."
            << SNAP_LOG_SEND;
        return;
    }

    name = name.substr(0, name.length() - 4) + "-*.ini";
    snapdev::glob_to_list<std::list<std::string>> glob;
    if(!glob.read_path<
             snapdev::glob_to_list_flag_t::GLOB_FLAG_IGNORE_ERRORS,
             snapdev::glob_to_list_flag_t::GLOB_FLAG_PERIOD>(name))
    {
        return;
    }

    for(auto const & n : glob)
    {
        SNAP_LOG_CONFIGURATION
            << "loading additional command line options from \""
            << n
            << "\".\n"
            << SNAP_LOG_SEND;
        f_opts.parse_options_from_file(n, 1, 1);
    }
}


/** \brief Finish sitter initialization and start the event loop.
 *
 * This function finishes the initialization such as defining the
 * server name, check that cassandra is available, and create various
 * connections such as the messenger to communicate with the
 * communicatord service.
 */
int server::run()
{
    // TODO: test that the "sites" table is available?
    //       (we will not need any such table here)

    // capture Ctrl-C (SIGINT)
    //
    f_interrupt = std::make_shared<interrupt>(this);
    f_communicator->add_connection(f_interrupt);

    // create the messenger, a connection between the sitter
    // and the communicatord which allows us to communicate
    // to any running services
    //
    f_communicator->add_connection(f_messenger);
    f_messenger->finish_initialization(shared_from_this());

    // add the ticker, this wakes the system up once in a while so
    // we can gather statistics at a given interval
    //
    f_tick_timer = std::make_shared<tick_timer>(this);
    f_communicator->add_connection(f_tick_timer);

    // start runner thread
    //
    f_worker_done = std::make_shared<worker_done>(this);
    f_communicator->add_connection(f_worker_done);
    f_worker = std::make_shared<sitter_worker>(shared_from_this(), f_worker_done);
    f_worker_thread = std::make_shared<cppthread::thread>("worker", f_worker);
    f_worker_thread->start();

    // now start the run() loop
    //
    f_communicator->run();

    // got a RELOADCONFIG message?
    // (until our daemons are capable of reloading configuration files
    // or rather, until we have the `fluid-settings` daemon)
    //
    return f_force_restart ? 2 : 0;
}


/** \brief Send a message via the messenger.
 *
 * This function is an override which allows the sitter server to
 * handle messages through the dispatcher.
 */
bool server::send_message(ed::message & message, bool cache)
{
    return f_messenger->send_message(message, cache);
}


/** \brief Process one tick.
 *
 * This function is called once a minute (by default). It goes and gather
 * all the data from all the plugins and then save that in the database.
 *
 * In case the tick happens too often, the function makes sure that the
 * child process is started at most once.
 */
void server::process_tick()
{
    f_worker->tick();
}


//bool server::init_parameters()
//{
// the below code worked when we had all the parameters at hand, now that
// we receive some of that data via fluid-settings, we may often not have a
// fully stable state meaning that the tests would generate errors that are
// just about to be fixed...
//
//    // now that all the priority & span numbers are defined we can verify
//    // that they are properly sorted
//    //
//    if(f_error_report_medium_priority < f_error_report_low_priority)
//    {
//        SNAP_LOG_FATAL
//            << "error report medium priority ("
//            << f_error_report_medium_priority
//            << ") cannot be less than the low priority ("
//            << f_error_report_low_priority
//            << ")."
//            << SNAP_LOG_SEND;
//        return false;
//    }
//    if(f_error_report_critical_priority < f_error_report_medium_priority)
//    {
//        SNAP_LOG_FATAL
//            << "error report critical priority ("
//            << f_error_report_critical_priority
//            << ") cannot be less than the medium priority ("
//            << f_error_report_medium_priority
//            << ")."
//            << SNAP_LOG_SEND;
//        return false;
//    }
//
//    if(f_error_report_medium_span > f_error_report_low_span)
//    {
//        SNAP_LOG_FATAL
//            << "error report medium span ("
//            << f_error_report_medium_span
//            << ") cannot be more than the low span ("
//            << f_error_report_low_span
//            << ")."
//            << SNAP_LOG_SEND;
//        return false;
//    }
//    if(f_error_report_critical_span > f_error_report_medium_span)
//    {
//        SNAP_LOG_FATAL
//            << "error report critical span ("
//            << f_error_report_critical_span
//            << ") cannot be more than the medium span ("
//            << f_error_report_medium_span
//            << ")."
//            << SNAP_LOG_SEND;
//        return false;
//    }
//
//    return true;
//}



void server::msg_rusage(ed::message & message)
{
    record_usage(message);
}


void server::ready(ed::message & message)
{
    // WARNING: the sitter is unusual as we derive the server class from
    //          ed::dispatcher which is not usually the way to do it; instead
    //          you want to look at doing so from the messenger and that way
    //          you get the ready() call as expected to the fluid-settings
    //          connection; in this case we instead have to call that other
    //          implementation explicitly
    //
    f_messenger->ready(message);

    set_communicatord_connected(true);

//    // TBD: should we wait on this signal before we start the g_tick_timer?
//    //      since we do not need the snap communicator, probably not useful
//    //      (however, we like to have Cassandra and we know Cassandra is
//    //      ready only after a we got the CASSANDRAREADY anyway...)
//    //
//
//    // request snapdbproxy to send us a status signal about
//    // Cassandra, after that one call, we will receive the
//    // changes in status just because we understand them.
//    //
//    ed::message isdbready_message;
//    isdbready_message.set_command("CASSANDRASTATUS");
//    isdbready_message.set_service("snapdbproxy");
//    f_messenger->send_message(isdbready_message);
}


void server::fluid_ready()
{
    f_tick_timer->set_enable(true);
}


void server::msg_reload_config(ed::message & message)
{
    snapdev::NOT_USED(message);

    f_force_restart = true;
    stop(false);
}




/** \brief Get the amount of time to wait between attempt at gathering stats.
 *
 * This value is the amount of time between statistics gatherings. The
 * amount of time it takes to gather the statistics is not included. So
 * if it takes 20 seconds to gather the statistics and the gathering is
 * set at 3 minutes, then once done gathering statistics we wait another
 * 3 minutes - 20 seconds (2 minutes and 40 seconds) before the next
 * gathering.
 *
 * The function caches the data. When the value changes, the fluid status
 * makes sure to clear the cached value.
 *
 * \return The duration in seconds.
 */
std::int64_t server::get_statistics_frequency()
{
    if(f_statistics_frequency <= 0)
    {
        std::string const statistics_frequency_str(f_opts.get_string("statistics-frequency"));
        std::int64_t statistics_frequency(DEFAULT_STATISTICS_FREQUENCY);
        double duration(0.0);
        if(advgetopt::validator_duration::convert_string(
                  statistics_frequency_str
                , advgetopt::validator_duration::VALIDATOR_DURATION_DEFAULT_FLAGS
                , duration))
        {
            statistics_frequency = static_cast<std::int64_t>(ceil(duration));
            if(statistics_frequency < 0)
            {
                SNAP_LOG_RECOVERABLE_ERROR
                    << "statistic frequency ("
                    << statistics_frequency_str
                    << ") cannot be a negative number. Using default instead."
                    << SNAP_LOG_SEND;
                statistics_frequency = DEFAULT_STATISTICS_FREQUENCY;
            }
        }
        else
        {
            SNAP_LOG_RECOVERABLE_ERROR
                << "statistic frequency ("
                << statistics_frequency_str
                << ") is not a valid duration. Using default instead."
                << SNAP_LOG_SEND;
        }

        // minimum is 1 minute
        //
        f_statistics_frequency = std::max(MINIMUM_STATISTICS_FREQUENCY, statistics_frequency);
    }

    return f_statistics_frequency;
}


/** \brief Get the period of time for which the statistics are kept.
 *
 * The statistics are saved in files. After a while, we delete old files.
 * This value defines how old the oldest statistics kept can be.
 *
 * \return The statistics period.
 */
std::int64_t server::get_statistics_period()
{
    if(f_statistics_period <= 0)
    {
        std::int64_t statistics_period(DEFAULT_STATISTICS_PERIOD);
        std::string const statistics_period_str(f_opts.get_string("statistics-period"));
        double duration(0.0);
        if(advgetopt::validator_duration::convert_string(
                  statistics_period_str
                , advgetopt::validator_duration::VALIDATOR_DURATION_DEFAULT_FLAGS
                , duration))
        {
            statistics_period = static_cast<std::int64_t>(ceil(duration));
            if(statistics_period < 0)
            {
                SNAP_LOG_RECOVERABLE_ERROR
                    << "statistic period ("
                    << statistics_period_str
                    << ") cannot be a negative number."
                    << SNAP_LOG_SEND;
                statistics_period = DEFAULT_STATISTICS_PERIOD;
            }
        }
        else
        {
            SNAP_LOG_RECOVERABLE_ERROR
                << "statistics-period \""
                << statistics_period_str
                << "\" is not a valid duration."
                << SNAP_LOG_SEND;
        }
        statistics_period = std::max(MINIMUM_STATISTICS_PERIOD, statistics_period);

        // round up
        //
        f_statistics_period = (statistics_period + (ROUND_STATISTICS_PERIOD - 1)) / ROUND_STATISTICS_PERIOD * ROUND_STATISTICS_PERIOD;
    }

    return f_statistics_period;
}


/** \brief Time To Live.
 *
 * The Time to Live (TTL) is used to make sure we do not overcrowd the
 * database. This can be turned off ("off") or marked to make use of
 * the exact same amount as defined in the statistics-period ("use-period").
 * Otherwise, it must be a duration representing the time to live.
 *
 * \note
 * At this time, we may not use this value depending on where the values
 * get saved. (i.e. we are not using Casandra anymore)
 *
 * \note
 * Internally, "off" is represented by 0.
 *
 * \return The maximum time to live in the database.
 */
std::int64_t server::get_statistics_ttl()
{
    if(f_statistics_ttl < 0)
    {
        std::string const statistics_ttl_str(f_opts.get_string("statistics-ttl"));
        if(statistics_ttl_str == "off")
        {
            f_statistics_ttl = 0;
        }
        else if(statistics_ttl_str == "use-period")
        {
            f_statistics_ttl = get_statistics_period();
        }
        else
        {
            std::int64_t statistics_ttl(DEFAULT_STATISTICS_TTL);
            double duration(0.0);
            if(advgetopt::validator_duration::convert_string(
                      statistics_ttl_str
                    , advgetopt::validator_duration::VALIDATOR_DURATION_DEFAULT_FLAGS
                    , duration))
            {
                statistics_ttl = static_cast<std::int64_t>(ceil(duration));
                if(statistics_ttl < 0)
                {
                    SNAP_LOG_RECOVERABLE_ERROR
                        << "statistic ttl ("
                        << statistics_ttl_str
                        << ") cannot be a negative number."
                        << SNAP_LOG_SEND;
                    statistics_ttl = DEFAULT_STATISTICS_TTL;
                }
            }
            else
            {
                SNAP_LOG_RECOVERABLE_ERROR
                    << "statistic ttl \""
                    << statistics_ttl
                    << "\" is not a valid number."
                    << SNAP_LOG_SEND;
            }

            f_statistics_ttl = std::max(MINIMUM_STATISTICS_TTL, statistics_ttl);
        }
    }

    return f_statistics_ttl;
}


/** \brief Amount of time before we start sending reports by email
 *
 * Often the first few minutes can be hectic on a server since many things
 * start all at the same time. For that reason, we do not want to start
 * reporting issues just after a reboot. This duration defines the amount
 * of time to wait for things to settle.
 *
 * \return The duration since starting this process to wait before starting
 * to send emails on errors.
 */
std::int64_t server::get_error_report_settle_time()
{
    if(f_error_report_settle_time < 0)
    {
        std::int64_t error_report_settle_time(DEFAULT_ERROR_REPORT_SETTLE_TIME);
        std::string const error_report_settle_time_str(f_opts.get_string("error-report-settle-time"));
        double duration(0.0);
        if(advgetopt::validator_duration::convert_string(
                  error_report_settle_time_str
                , advgetopt::validator_duration::VALIDATOR_DURATION_DEFAULT_FLAGS
                , duration))
        {
            error_report_settle_time = static_cast<std::int64_t>(ceil(duration));
            if(error_report_settle_time < 0)
            {
                SNAP_LOG_RECOVERABLE_ERROR
                    << "error report settle time ("
                    << error_report_settle_time_str
                    << ") cannot be a negative number."
                    << SNAP_LOG_SEND;
                error_report_settle_time = DEFAULT_ERROR_REPORT_SETTLE_TIME;
            }
            f_error_report_settle_time = std::max(MINIMUM_ERROR_REPORT_SETTLE_TIME, error_report_settle_time);
            // TBD: should we have a maximum like 1h?
        }
        else
        {
            SNAP_LOG_RECOVERABLE_ERROR
                << "error report settle time ("
                << error_report_settle_time_str
                << ") is not a valid duration."
                << SNAP_LOG_SEND;
        }
    }

    return f_error_report_settle_time;
}


/** \brief Low priority and span.
 *
 * Define what is considered low priority. This allows us to avoid error
 * messages when a small issue appears. The issue may disappear opn its own
 * with time, in which case it was not much of an issue, or it will be taken
 * care of whenever the administrator checks the system closely.
 *
 * \return The low priority of error reports.
 */
std::int64_t server::get_error_report_low_priority()
{
    if(f_error_report_low_priority < 0)
    {
        f_error_report_low_priority = DEFAULT_ERROR_REPORT_LOW_PRIORITY;
        f_error_report_low_span = DEFAULT_ERROR_REPORT_LOW_SPAN;

        std::string const low_priority_str(f_opts.get_string("error-report-low-priority"));
        if(!low_priority_str.empty())
        {
            advgetopt::string_list_t prio_span;
            advgetopt::split_string(low_priority_str, prio_span, {","});
            if(prio_span.size() > 2)
            {
                SNAP_LOG_RECOVERABLE_ERROR
                    << "error report low priority \""
                    << low_priority_str
                    << "\" is expect to have two numbers separated by one comma. The second number is optional."
                    << SNAP_LOG_SEND;
                // ignore the 3rd, etc.
            }

            // LOW PRIORITY
            //
            if(advgetopt::validator_integer::convert_string(
                          prio_span[0]
                        , f_error_report_low_priority))
            {
                if(f_error_report_low_priority < MINIMUM_ERROR_REPORT_LOW_PRIORITY)
                {
                    SNAP_LOG_RECOVERABLE_ERROR
                        << "error report low priority ("
                        << low_priority_str
                        << ") cannot be less than "
                        << MINIMUM_ERROR_REPORT_LOW_PRIORITY
                        << "."
                        << SNAP_LOG_SEND;
                    f_error_report_low_priority = MINIMUM_ERROR_REPORT_LOW_PRIORITY;
                }
                else if(f_error_report_low_priority > MAXIMUM_ERROR_REPORT_LOW_PRIORITY)
                {
                    SNAP_LOG_RECOVERABLE_ERROR
                        << "error report low priority ("
                        << low_priority_str
                        << ") cannot be more than "
                        << MAXIMUM_ERROR_REPORT_LOW_PRIORITY
                        << "."
                        << SNAP_LOG_SEND;
                    f_error_report_low_priority = MAXIMUM_ERROR_REPORT_LOW_PRIORITY;
                }
            }
            else
            {
                SNAP_LOG_RECOVERABLE_ERROR
                    << "error report low priority \""
                    << low_priority_str
                    << "\" is not a valid number."
                    << SNAP_LOG_SEND;
            }

            // LOW SPAN
            //
            if(prio_span.size() >= 2
            && !prio_span[1].empty())
            {
                double duration(0.0);
                if(advgetopt::validator_duration::convert_string(
                              prio_span[1]
                            , advgetopt::validator_duration::VALIDATOR_DURATION_DEFAULT_FLAGS
                            , duration))
                {
                    f_error_report_low_span = static_cast<std::int64_t>(ceil(duration));
                    if(f_error_report_low_span <= 0)
                    {
                        SNAP_LOG_FATAL
                            << "error report low span ("
                            << low_priority_str
                            << ") cannot be negative or null."
                            << SNAP_LOG_SEND;
                        f_error_report_low_span = DEFAULT_ERROR_REPORT_LOW_SPAN;
                    }
                    else if(f_error_report_low_span < MINIMUM_ERROR_REPORT_LOW_SPAN)
                    {
                        f_error_report_low_span = MINIMUM_ERROR_REPORT_LOW_SPAN;
                    }
                }
                else
                {
                    SNAP_LOG_FATAL
                        << "error report low span \""
                        << low_priority_str
                        << "\" is not a valid number."
                        << SNAP_LOG_SEND;
                }
            }
        }
    }

    return f_error_report_low_priority;
}


std::int64_t server::get_error_report_low_span()
{
    snapdev::NOT_USED(get_error_report_low_priority());
    return f_error_report_low_span;
}


std::int64_t server::get_error_report_medium_priority()
{
    if(f_error_report_medium_priority < 0)
    {
        f_error_report_medium_priority = DEFAULT_ERROR_REPORT_MEDIUM_PRIORITY;
        f_error_report_medium_span = DEFAULT_ERROR_REPORT_MEDIUM_SPAN;
        std::string const medium_priority_str(f_opts.get_string("error-report-medium-priority"));
        if(!medium_priority_str.empty())
        {
            advgetopt::string_list_t prio_span;
            advgetopt::split_string(medium_priority_str, prio_span, {","});
            if(prio_span.size() > 2)
            {
                SNAP_LOG_RECOVERABLE_ERROR
                    << "error report medium priority \""
                    << medium_priority_str
                    << "\" is expect to have two numbers separated by one comma. The second number is optional."
                    << SNAP_LOG_SEND;
                // ignore the 3rd, etc.
            }

            if(advgetopt::validator_integer::convert_string(
                          prio_span[0]
                        , f_error_report_medium_priority))
            {
                if(f_error_report_medium_priority < MINIMUM_ERROR_REPORT_MEDIUM_PRIORITY)
                {
                    // TBD: should we just clamp silently instead of a fatal error?
                    //
                    SNAP_LOG_RECOVERABLE_ERROR
                        << "error report medium priority ("
                        << medium_priority_str
                        << ") cannot be larger than "
                        << MAXIMUM_ERROR_REPORT_MEDIUM_PRIORITY
                        << "."
                        << SNAP_LOG_SEND;
                    f_error_report_medium_priority = MINIMUM_ERROR_REPORT_MEDIUM_PRIORITY;
                }
                else if(f_error_report_medium_priority > MAXIMUM_ERROR_REPORT_MEDIUM_PRIORITY)
                {
                    // TBD: should we just clamp silently instead of a fatal error?
                    //
                    SNAP_LOG_RECOVERABLE_ERROR
                        << "error report medium priority ("
                        << medium_priority_str
                        << ") cannot be larger than "
                        << MAXIMUM_ERROR_REPORT_MEDIUM_PRIORITY
                        << "."
                        << SNAP_LOG_SEND;
                    f_error_report_medium_priority = MAXIMUM_ERROR_REPORT_MEDIUM_PRIORITY;
                }
            }
            else
            {
                SNAP_LOG_RECOVERABLE_ERROR
                    << "error report medium priority \""
                    << medium_priority_str
                    << "\" is not a valid number."
                    << SNAP_LOG_SEND;
            }

            if(prio_span.size() >= 2
            && !prio_span[1].empty())
            {
                double duration(0.0);
                if(advgetopt::validator_duration::convert_string(
                              prio_span[1]
                            , advgetopt::validator_duration::VALIDATOR_DURATION_DEFAULT_FLAGS
                            , duration))
                {
                    f_error_report_medium_span = static_cast<std::int64_t>(ceil(duration));
                    if(f_error_report_medium_span < 0)
                    {
                        SNAP_LOG_RECOVERABLE_ERROR
                            << "error report medium span ("
                            << medium_priority_str
                            << ") cannot be negative."
                            << SNAP_LOG_SEND;
                        f_error_report_medium_span = DEFAULT_ERROR_REPORT_MEDIUM_SPAN;
                    }
                    else if(f_error_report_medium_span < MINIMUM_ERROR_REPORT_MEDIUM_SPAN)
                    {
                        f_error_report_medium_span = MINIMUM_ERROR_REPORT_MEDIUM_SPAN;
                    }
                }
                else
                {
                    SNAP_LOG_RECOVERABLE_ERROR
                        << "error report medium span \""
                        << medium_priority_str
                        << "\" is not a valid number."
                        << SNAP_LOG_SEND;
                }
            }
        }
    }

    return f_error_report_medium_priority;
}


std::int64_t server::get_error_report_medium_span()
{
    snapdev::NOT_USED(get_error_report_medium_priority());
    return f_error_report_medium_span;
}


int64_t server::get_error_report_critical_priority()
{
    if(f_error_report_critical_priority < 0)
    {
        f_error_report_critical_priority = DEFAULT_ERROR_REPORT_CRITICAL_PRIORITY;
        f_error_report_critical_span = DEFAULT_ERROR_REPORT_CRITICAL_SPAN;
        std::string const critical_priority_str(f_opts.get_string("error-report-critical-priority"));
        if(!critical_priority_str.empty())
        {
            advgetopt::string_list_t prio_span;
            advgetopt::split_string(critical_priority_str, prio_span, {","});
            if(prio_span.size() > 2)
            {
                SNAP_LOG_RECOVERABLE_ERROR
                    << "error report critical priority \""
                    << critical_priority_str
                    << "\" is expect to have two numbers separated by one comma. The second number is optional."
                    << SNAP_LOG_SEND;
                return false;
            }

            if(advgetopt::validator_integer::convert_string(
                          prio_span[0]
                        , f_error_report_critical_priority))
            {
                if(f_error_report_critical_priority < MINIMUM_ERROR_REPORT_CRITICAL_PRIORITY)
                {
                    SNAP_LOG_RECOVERABLE_ERROR
                        << "error report critical priority ("
                        << critical_priority_str
                        << ") cannot be less than "
                        << MINIMUM_ERROR_REPORT_CRITICAL_PRIORITY
                        << "."
                        << SNAP_LOG_SEND;
                    f_error_report_critical_priority = DEFAULT_ERROR_REPORT_CRITICAL_PRIORITY;
                }
                else if(f_error_report_critical_priority > MAXIMUM_ERROR_REPORT_CRITICAL_PRIORITY)
                {
                    // TBD: should we just clamp silently instead of a fatal error?
                    //
                    SNAP_LOG_RECOVERABLE_ERROR
                        << "error report critical priority ("
                        << critical_priority_str
                        << ") cannot be larger than "
                        << MAXIMUM_ERROR_REPORT_CRITICAL_PRIORITY
                        << "."
                        << SNAP_LOG_SEND;
                    f_error_report_critical_priority = DEFAULT_ERROR_REPORT_CRITICAL_PRIORITY;
                }
            }
            else
            {
                SNAP_LOG_RECOVERABLE_ERROR
                    << "error report critical priority \""
                    << critical_priority_str
                    << "\" is not a valid number."
                    << SNAP_LOG_SEND;
            }

            if(prio_span.size() == 2
            && !prio_span[1].empty())
            {
                double duration(0.0);
                if(advgetopt::validator_duration::convert_string(
                              prio_span[1]
                            , advgetopt::validator_duration::VALIDATOR_DURATION_DEFAULT_FLAGS
                            , duration))
                {
                    f_error_report_critical_span = static_cast<std::int64_t>(ceil(duration));
                    if(f_error_report_critical_span < 0)
                    {
                        SNAP_LOG_RECOVERABLE_ERROR
                            << "error report critical span ("
                            << critical_priority_str
                            << ") cannot be negative."
                            << SNAP_LOG_SEND;
                        f_error_report_critical_span = DEFAULT_ERROR_REPORT_CRITICAL_SPAN;
                    }
                    else if(f_error_report_critical_span < MINIMUM_ERROR_REPORT_CRITICAL_SPAN)
                    {
                        f_error_report_critical_span = MINIMUM_ERROR_REPORT_CRITICAL_SPAN;
                    }
                }
                else
                {
                    SNAP_LOG_RECOVERABLE_ERROR
                        << "error report critical span \""
                        << critical_priority_str
                        << "\" is not a valid number."
                        << SNAP_LOG_SEND;
                }
            }
        }
    }

    return f_error_report_critical_priority;
}


std::int64_t server::get_error_report_critical_span()
{
    snapdev::NOT_USED(get_error_report_critical_priority());
    return f_error_report_critical_span;
}


void server::set_ticks(int ticks)
{
    f_ticks = ticks;
}


int server::get_ticks() const
{
    return f_ticks;
}



void server::clear_cache(std::string const & name)
{
    if(name.empty())
    {
        return;
    }

    switch(name[0])
    {
    case 'e':
        if(name == "error-report-settle-time")
        {
            f_error_report_settle_time = -1;
        }
        else if(name == "error-report-low-priority")
        {
            f_error_report_low_priority = -1;
            f_error_report_low_span = -1;
        }
        else if(name == "error-report-medium-priority")
        {
            f_error_report_medium_priority = -1;
            f_error_report_medium_span = -1;
        }
        else if(name == "error-report-critical-priority")
        {
            f_error_report_critical_priority = -1;
            f_error_report_critical_span = -1;
        }
        break;

    case 's':
        if(name == "statistics-frequency")
        {
            f_statistics_frequency = -1;
        }
        else if(name == "statistics-period")
        {
            f_statistics_period = -1;

            // the TTL may make use of the statistics period so we need to
            // reset that one too in this case
            //
            f_statistics_ttl = -1;
        }
        else if(name == "statistics-ttl")
        {
            f_statistics_ttl = -1;
        }
        break;

    }
}


bool server::output_process(
      std::string const & plugin_name
    , as2js::json::json_value_ref & json
    , cppprocess::process_info::pointer_t info
    , std::string const & process_name
    , int priority)
{
    as2js::json::json_value_ref process(json["process"][-1]);
    process["name"] = process_name;

    if(info == nullptr)
    {
        // no communicatord process!?
        //
        process["error"] = "missing";

        append_error(
                  json
                , plugin_name
                , "can't find mandatory processs \""
                    + process_name
                    + "\" in the list of processes. network health is not available."
                , priority);

        return false;
    }

    // got it! (well, one of them at least)
    //
    process["cmdline"] = info->get_command();
    process["pcpu"] = info->get_cpu_percent();
    process["total_size"] = info->get_total_size();
    process["resident"] = info->get_rss_size();

    int tty_major(0);
    int tty_minor(0);
    info->get_tty(tty_major, tty_minor);
    process["tty"] = std::to_string(tty_major) + ',' + std::to_string(tty_minor);

    unsigned long long utime(0);
    unsigned long long stime(0);
    unsigned long long cutime(0);
    unsigned long long cstime(0);
    info->get_times(utime, stime, cutime, cstime);

    process["utime"] = std::to_string(utime);
    process["stime"] = std::to_string(stime);
    process["cutime"] = std::to_string(cutime);
    process["cstime"] = std::to_string(cstime);

    return true;
}






void server::stop(bool quitting)
{
    SNAP_LOG_INFO
        << "Stopping sitter server."
        << SNAP_LOG_SEND;

    f_stopping = true;

    if(f_worker != nullptr
    && f_worker_thread != nullptr
    && f_worker_thread->is_running())
    {
        f_worker_thread->stop([&](cppthread::thread * t){
                snapdev::NOT_USED(t);
                f_worker->wakeup();
            });
        f_worker_thread.reset();
        f_worker.reset();
    }

    if(f_messenger != nullptr)
    {
        f_messenger->unregister_communicator(quitting);

        // we can remove our messenger immediately, the communicator lower
        // layer is responsible for sending messages, etc.
        //
        f_communicator->remove_connection(f_messenger);
    }

    f_communicator->remove_connection(f_interrupt);
    f_communicator->remove_connection(f_tick_timer);
    f_communicator->remove_connection(f_worker_done);
}



void server::set_communicatord_connected(bool status)
{
    if(status)
    {
        f_communicatord_connected = snapdev::timespec_ex::gettime();
    }
    else
    {
        f_communicatord_disconnected = snapdev::timespec_ex::gettime();
    }
}



bool server::get_communicatord_is_connected() const
{
    return f_communicatord_disconnected < f_communicatord_connected;
}



snapdev::timespec_ex server::get_communicatord_connected_on() const
{
    return f_communicatord_connected;
}



snapdev::timespec_ex server::get_communicatord_disconnected_on() const
{
    return f_communicatord_disconnected;
}


std::string server::get_server_parameter(std::string const & name) const
{
    if(f_opts.is_defined(name))
    {
        return f_opts.get_string(name);
    }

    return std::string();
}


/** \brief Get the path to a file in the sitter cache.
 *
 * This function returns a full path to the sitter cache plus
 * the specified \p filename.
 *
 * \p filename should just be a filename. i.e. it should not
 * include any slashes. It should also be unique to your plugin.
 *
 * \param[in] filename  The name of the file to access in the cache.
 *
 * \return The full path including your filename.
 */
std::string server::get_cache_path(std::string const & filename)
{
    if(f_cache_path.empty())
    {
        // get the path specified by the administrator or default
        //
        f_cache_path = f_opts.get_string("cache-path");

        // the path to "/var/cache/sitter" should always exist
        //
        // if the user defined a different path, try to create it if it does
        // not exist (in all likelihood, it fails because permissions prevent
        // the creation of the directory)
        //
        if(!snapdev::mkdir_p(f_cache_path))
        {
            f_cache_path.clear();
            return std::string();
        }
    }

    // append the name of the file to check out in the path
    //
    return f_cache_path + '/' + filename;
}


/** \brief Process an RUSAGE message.
 *
 * This function processes an RUSAGE message.
 *
 * \todo
 * We may want to look into having a binary format instead of JSON.
 *
 * \param[in] message  The message we just received.
 */
void server::record_usage(ed::message const & message)
{
    std::string const data_path(get_server_parameter(g_name_sitter_data_path));
    if(data_path.empty())
    {
        return;
    }

    as2js::json json;

    as2js::json::json_value_ref e(json["rusage"]);

    std::string const process_name(message.get_parameter("process_name"));
    std::string const pid(message.get_parameter("pid"));
    e["process_name"] =                 process_name;
    e["pid"] =                          pid;
    e["user_time"] =                    message.get_parameter("user_time");
    e["system_time"] =                  message.get_parameter("system_time");
    e["maxrss"] =                       message.get_parameter("maxrss");
    e["minor_page_fault"] =             message.get_parameter("minor_page_fault");
    e["major_page_fault"] =             message.get_parameter("major_page_fault");
    e["in_block"] =                     message.get_parameter("in_block");
    e["out_block"] =                    message.get_parameter("out_block");
    e["volontary_context_switches"] =   message.get_parameter("volontary_context_switches");
    e["involontary_context_switches"] = message.get_parameter("involontary_context_switches");

    time_t const start_date(time(nullptr));

    // add the date to this result
    //
    e["date"] = start_date;

    std::string const data(json.get_value()->to_string());

    // save data
    //
    std::string filename(data_path);
    filename += "/rusage/";
    if(snapdev::mkdir_p(filename, false, 0755, "sitter", "sitter") != 0)
    {
        SNAP_LOG_MAJOR
            << "sitter::record_usage(): could not create sub-directory  \""
            << filename
            << "\"."
            << SNAP_LOG_SEND;
        return;
    }
    filename += process_name;
    filename += '-';
    filename += (start_date / 3600) % 24;
    filename += ".json";

    snapdev::file_contents out(filename);
    out.contents(data);
    if(!out.write_all())
    {
        SNAP_LOG_WARNING
            << "sitter::record_usage(): could not save data to \""
            << filename
            << "\"."
            << SNAP_LOG_SEND;
        return;
    }
}


/** \brief Mark the server as not having errors.
 *
 * This function clears the "has errors" flag to false. It gets called before
 * the plugins process_watch().
 */
void server::clear_errors()
{
    f_error_count = 0;
    f_max_error_priority = 0;
}


/** \brief Attach an error to the specified \p doc DOM.
 *
 * This function creates an \<error> element and add the specified
 * message to it. The message can be any text you'd like.
 *
 * The plugin_name is expected to match the name of your plugin one to one.
 *
 * The priority is used to know whether an email will be sent to the user
 * or not. By default it is 50 and the configuration file says to send
 * emails if the priority is 1 or more. We expect numbers between 0 and 100.
 *
 * \param[in] json_ref  The JSON document where the \<error> tag is created.
 * \param[in] plugin_name  The name of the plugin generating this error.
 * \param[in] message  The error message. This is free form. It can't include
 *            tags (\< and \> will be inserted as \&lt; and \&gt;.)
 * \param[in] priority  The priority of this error. The higher the greater the
 *            priority and the sooner the error will be sent to the
 *            administrator.
 */
void server::append_error(
      as2js::json::json_value_ref & json_ref
    , std::string const & plugin_name
    , std::string const & message
    , int priority)
{
    if(priority > f_max_error_priority)
    {
        f_max_error_priority = priority;
    }
    ++f_error_count;

    // log the error so we have a trace
    //
    std::string clean_message(snapdev::string_replace_many(
              message
            , { { "\n", " -- " } }));
    SNAP_LOG_ERROR
        << "plugin \""
        << plugin_name
        << "\" detected an error: "
        << clean_message
        << " ("
        << priority
        << ")"
        << SNAP_LOG_SEND;

    if(priority < 0 || priority > 100)
    {
        throw invalid_parameter(
                  "priority must be between 0 and 100 inclusive, "
                + std::to_string(priority)
                + " is not valid.");
    }

    // create a new item in the array (at the end)
    //
    as2js::json::json_value_ref err(json_ref["error"][-1]);
    err["plugin_name"] = plugin_name;
    err["message"] = message;
    err["priority"] = priority;
}


int server::get_error_count() const
{
    return f_error_count;
}


int server::get_max_error_priority() const
{
    return f_max_error_priority;
}



} // namespace sitter
// vim: ts=4 sw=4 et
