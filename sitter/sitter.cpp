// Copyright (c) 2011-2022  Made to Order Software Corp.  All Rights Reserved.
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


// libmimemail
//
#include    <libmimemail/email.h>


// snaplogger
//
#include    <snaplogger/logger.h>
#include    <snaplogger/message.h>
#include    <snaplogger/options.h>


// advgetopt
//
#include    <advgetopt/conf_file.h>
#include    <advgetopt/exception.h>
#include    <advgetopt/validator_integer.h>


// libaddr
//
#include    <libaddr/addr_parser.h>


// eventdispatcher
//
#include    <eventdispatcher/signal.h>
#include    <eventdispatcher/tcp_client_permanent_message_connection.h>


// snapdev
//
#include    <snapdev/file_contents.h>
#include    <snapdev/gethostname.h>
#include    <snapdev/mkdir_p.h>
#include    <snapdev/not_reached.h>
#include    <snapdev/not_used.h>
#include    <snapdev/string_replace_many.h>


// C++
//
#include    <algorithm>
#include    <fstream>


// C
//
#include    <sys/wait.h>


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


/** \brief The ed::communicator singleton.
 *
 * This variable holds a copy of the ed::communicator singleton.
 * It is a null pointer until the sitter gets initialized.
 */
ed::communicator::pointer_t     g_communicator;


/** \brief The gathering of data uses a thread now.
 *
 * In the first version, we used a fork() and loaded the plugins in the
 * child process.
 *
 * In the new version, we start a thread early one, load the plugins once,
 * and then sleep until we get a tick. The process runs until the service
 * quits.
 */
cppthread::thread::pointer_t    g_worker_thread;



advgetopt::option const g_command_line_options[] =
{
    advgetopt::define_option(
          advgetopt::Name("administrator-email")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::Validator("email(single)")
        , advgetopt::Help("the email address of the administrator to email whenever an issue is detected.")
    ),
    advgetopt::define_option(
          advgetopt::Name("cache-path")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::DefaultValue("/var/cache/sitter")
        , advgetopt::Help("the path to the cache used by the sitter.")
    ),
    advgetopt::define_option(
          advgetopt::Name("data-path")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::DefaultValue("/var/lib/sitter")
        , advgetopt::Help("the path to a directory where plugins can save data.")
    ),
    advgetopt::define_option(
          advgetopt::Name("disk-ignore")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::Help("colon separated regular expressions defining paths ignored when checking disks.")
    ),
    advgetopt::define_option(
          advgetopt::Name("error-report-critical-priority")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::DefaultValue("90,86400")
        , advgetopt::Help("the critical priority a message has to trigger an email after the specified period (priority and period are separated by a comma).")
    ),
    advgetopt::define_option(
          advgetopt::Name("error-report-low-priority")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::DefaultValue("10,604800")
        , advgetopt::Help("the minimum priority a message has to trigger an email after the specified period (priority and period are separated by a comma).")
    ),
    advgetopt::define_option(
          advgetopt::Name("error-report-medium-priority")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::DefaultValue("50,259200")
        , advgetopt::Help("the medium priority a message has to trigger an email after the specified period (priority and period are separated by a comma).")
    ),
    advgetopt::define_option(
          advgetopt::Name("error-report-settle-time")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::DefaultValue("300")
        , advgetopt::Validator("duration")
        , advgetopt::Help("the amount of time the sitter waits before sending reports; this gives the server time to get started.")
    ),
    advgetopt::define_option(
          advgetopt::Name("from-email")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::Help("the email address to use in the \"From: ...\" field when sending emails.")
    ),
    advgetopt::define_option(
          advgetopt::Name("plugins")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::DefaultValue("apt,cpu,disk,flags,log,memory,network,packages,processes,scripts")
        , advgetopt::Help("the list of sitter plugins to run.")
    ),
    advgetopt::define_option(
          advgetopt::Name("plugins-path")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::DefaultValue("/usr/lib/sitter/plugins")
        , advgetopt::Help("the path to the location holding the sitter plugins.")
    ),
    advgetopt::define_option(
          advgetopt::Name("statistics-frequency")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::DefaultValue("60")
        , advgetopt::Validator("duration")
        , advgetopt::Help("how often the sitter runs all the plugins.")
    ),
    advgetopt::define_option(
          advgetopt::Name("statistics-period")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::DefaultValue("604800")
        , advgetopt::Validator("duration")
        , advgetopt::Help("time for the statistics to live; older statistics get deleted.")
    ),
    advgetopt::define_option(
          advgetopt::Name("statistics-ttl")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::DefaultValue("off")
        , advgetopt::Validator("keyword(off) | duration")
        , advgetopt::Help("the statistics can be saved in the database in which case a TTL is assigned to that data so it automatically gets deleted; use \"off\" to turn off this feature.")
    ),
    advgetopt::define_option(
          advgetopt::Name("user-group")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::DefaultValue("sitter:sitter")
        , advgetopt::Help("the name of a user and a group, separated by a colon, to use for the statistics and other journal files.")
    ),
    advgetopt::end_options()
};





// until we have C++20 remove warnings this way
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
advgetopt::options_environment const g_options_environment =
{
    .f_project_name = "sitter",
    .f_group_name = nullptr,
    .f_options = g_command_line_options,
    .f_options_files_directory = nullptr,
    .f_environment_variable_name = nullptr,
    .f_environment_variable_intro = nullptr,
    .f_section_variables_name = nullptr,
    .f_configuration_files = nullptr,
    .f_configuration_filename = nullptr,
    .f_configuration_directories = nullptr,
    .f_environment_flags = advgetopt::GETOPT_ENVIRONMENT_FLAG_PROCESS_SYSTEM_PARAMETERS,
    .f_help_header = "Usage: %p [-<opt>] <process-name>\n"
                     "where -<opt> is one or more of:",
    .f_help_footer = "%c",
    .f_version = SITTER_VERSION_STRING,
    .f_license = "GNU GPL v2",
    .f_copyright = "Copyright (c) 2013-"
                   BOOST_PP_STRINGIZE(UTC_BUILD_YEAR)
                   " by Made to Order Software Corporation -- All Rights Reserved",
    //.f_build_date = UTC_BUILD_DATE,
    //.f_build_time = UTC_BUILD_TIME
};
#pragma GCC diagnostic pop



/** \brief Handle the SIGINT that is expected to stop the server.
 *
 * This class is an implementation of the snap_signal that listens
 * on the SIGINT.
 */
class interrupt
    : public ed::signal
{
public:
    typedef std::shared_ptr<interrupt>     pointer_t;

                        interrupt(server::pointer_t s);
    virtual             ~interrupt() override {}

    // ed::connection implementation
    virtual void        process_signal() override;

private:
    server::pointer_t  f_server = server::pointer_t();
};


/** \brief The tick timer.
 *
 * We create one tick timer. It is saved in this variable if needed.
 */
interrupt::pointer_t             g_interrupt;


/** \brief The interrupt initialization.
 *
 * The interrupt uses the signalfd() function to obtain a way to listen on
 * incoming Unix signals.
 *
 * Specifically, it listens on the SIGINT signal, which is the equivalent
 * to the Ctrl-C.
 *
 * \param[in] ws  The server we are listening for.
 */
interrupt::interrupt(server::pointer_t s)
    : signal(SIGINT)
    , f_server(s)
{
    unblock_signal_on_destruction();
    set_name("interrupt");
}


/** \brief Call the stop function of the snaplock object.
 *
 * When this function is called, the signal was received and thus we are
 * asked to quit as soon as possible.
 */
void interrupt::process_signal()
{
    // we simulate the STOP, so pass 'false' (i.e. not quitting)
    //
    f_server->stop(false);
}







/** \brief The timer to produce ticks once every minute.
 *
 * This timer is the one used to know when to gather data again.
 *
 * By default the interval is set to one minute, although it is possible
 * to change that amount in the configuration file.
 */
class tick_timer
    : public ed::timer
{
public:
    typedef std::shared_ptr<tick_timer>        pointer_t;

                                tick_timer(server::pointer_t s, int64_t interval);
    virtual                     ~tick_timer() override {}

    // ed::timer implementation
    virtual void                process_timeout() override;

private:
    server::pointer_t           f_server = server::pointer_t();
};


/** \brief The tick timer.
 *
 * We create one tick timer. It is saved in this variable if needed.
 */
tick_timer::pointer_t             g_tick_timer;


/** \brief Initializes the timer with a pointer to the snap backend.
 *
 * The constructor saves the pointer of the snap_backend object so
 * it can later be used when the process times out.
 *
 * The timer is setup to trigger immediately after creation.
 * This is what starts the snap backend process.
 *
 * \param[in] s  A pointer to the snap_backend object.
 * \param[in] interval  The amount to wait between ticks.
 */
tick_timer::tick_timer(server::pointer_t s, int64_t interval)
    : timer(interval)
    , f_server(s)
{
    set_name("tick_timer");

    // start right away, but we do not want to use snap_timer(0)
    // because otherwise we will not get ongoing ticks as expected
    //
    set_timeout_date(ed::get_current_date());
}


/** \brief The timeout happened.
 *
 * This function gets called once every minute (although the interval can
 * be changed, it is 1 minute by default). Whenever it happens, the
 * sitter runs all the plugins once.
 */
void tick_timer::process_timeout()
{
    f_server->process_tick();
}





/** \brief Handle messages from the communicatord server.
 *
 * This class is an implementation of the TCP client message connection
 * so we can handle incoming messages.
 */
class messenger
    : public ed::tcp_client_permanent_message_connection
{
public:
    typedef std::shared_ptr<messenger>    pointer_t;

                                messenger(server::pointer_t s, addr::addr const & a);
    virtual                     ~messenger() override {}

    // ed::tcp_client_permanent_message_connection implementation
    virtual void                process_connection_failed(std::string const & error_message) override;
    virtual void                process_connected() override;

private:
    // this is owned by a server function so no need for a smart pointer
    server::pointer_t           f_server = server::pointer_t();
};


/** \brief The messenger.
 *
 * We create only one messenger. It is saved in this variable.
 */
messenger::pointer_t             g_messenger;


/** \brief The messenger initialization.
 *
 * The messenger is a connection to the communicatord service.
 *
 * In most cases we receive STOP and LOG messages from it. We implement
 * a few other messages too (HELP, READY...)
 *
 * We use a permanent connection so if the communicatord restarts
 * for whatever reason, we reconnect automatically.
 *
 * \param[in] s  The sitter server we are listening for.
 * \param[in] address  The address to connect to, most often, 127.0.0.1:4040.
 */
messenger::messenger(server::pointer_t s, addr::addr const & address)
    : tcp_client_permanent_message_connection(
              address
            , ed::mode_t::MODE_PLAIN
            , ed::DEFAULT_PAUSE_BEFORE_RECONNECTING
            , false // do not use a separate thread, we do many fork()'s
            , "sitter")
    , f_server(s)
{
    set_name("messenger");
}


/** \brief The messenger could not connect to communicatord.
 *
 * This function is called whenever the messengers fails to
 * connect to the communicatord server. This could be
 * because communicatord is not running or because the
 * information given to the sitter is wrong...
 *
 * With systemd the communicatord should already be running
 * although this is not 100% guaranteed. So getting this
 * error from time to time is considered normal.
 *
 * \note
 * This error happens whenever the communicatod is upgraded
 * since the packager stops the process, upgrades, then restarts
 * it. The sitter automatically reconnects once possible.
 *
 * \param[in] error_message  An error message.
 */
void messenger::process_connection_failed(std::string const & error_message)
{
    SNAP_LOG_ERROR
        << "connection to communicatord failed ("
        << error_message
        << ")"
        << SNAP_LOG_SEND;

    // also call the default function, just in case
    tcp_client_permanent_message_connection::process_connection_failed(error_message);
    f_server->set_communicatord_connected(false);
}


/** \brief The connection was established with communicatord.
 *
 * Whenever the connection is establied with the communicatord,
 * this callback function is called.
 *
 * The messenger reacts by REGISTERing the "sitter" service with the
 * communicatord.
 */
void messenger::process_connected()
{
    tcp_client_permanent_message_connection::process_connected();
    register_service();

    //ed::message register_backend;
    //register_backend.set_command("REGISTER");
    //register_backend.add_parameter("service", "sitter");
    //register_backend.add_version_parameter();
    //send_message(register_backend);
    f_server->set_communicatord_connected(true);
}








/** \brief List of sitter commands
 *
 * The following table defines the commands understood by the sitter service
 * that are not defined as a default by the ed::dispatcher implementation.
 */
ed::dispatcher<server>::dispatcher_match::vector_t const g_sitter_service_messages =
{
    {
        "RELOADCONFIG"
      , &server::msg_reload_config
    },
    {
        "RUSAGE"
      , &server::msg_rusage
    }
};


} // no name namespace




/** \brief Initialize the sitter server.
 *
 * This constructor makes sure to setup the correct filename for the
 * sitter server configuration file.
 */
server::server(int argc, char * argv[])
    : dispatcher(this, g_sitter_service_messages)
    , f_opts(g_options_environment)
    , f_logrotate(f_opts, "127.0.0.1", 4988)
    , f_communicatord_disconnected(time(nullptr))
{
    snaplogger::add_logger_options(f_opts);
    f_logrotate.add_logrotate_options();
    f_opts.finish_parsing(argc, argv);
    if(!snaplogger::process_logger_options(f_opts, "/etc/sitter/logger"))
    {
        // exit on any error
        //
        throw advgetopt::getopt_exit("logger options generated an error.", 0);
    }
    f_logrotate.process_logrotate_options();
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


/** \brief Finish sitter initialization and start the event loop.
 *
 * This function finishes the initialization such as defining the
 * server name, check that cassandra is available, and create various
 * connections such as the messenger to communicate with the
 * communicatord service.
 */
int server::run()
{
    SNAP_LOG_INFO
        << "------------------------------------ sitter "
        << snapdev::gethostname()
        << " started."
        << SNAP_LOG_SEND;

    if(!init_parameters())
    {
        return 1;
    }

    // TODO: test that the "sites" table is available?
    //       (we will not need any such table here)

    g_communicator = ed::communicator::instance();

    // capture Ctrl-C (SIGINT)
    //
    g_interrupt.reset(new interrupt(instance()));
    g_communicator->add_connection(g_interrupt);

    // get the communicatord IP and port
    // TODO: switch to fluid-settings
    //
    advgetopt::conf_file_setup conf_setup("/etc/communicatod/communicatord.conf");
    advgetopt::conf_file::pointer_t communicator_settings(advgetopt::conf_file::get_conf_file(conf_setup));
    std::string const communicator_addr("127.0.0.1");
    constexpr int const communicator_port(4040);
    addr::addr const a(addr::string_to_addr(
              communicator_settings->get_parameter("local_listen")
            , communicator_addr
            , communicator_port
            , "tcp"));

    // create the messenger, a connection between the sitter
    // and the communicatord which allows us to communicate
    //
    g_messenger.reset(new messenger(instance(), a));
    g_communicator->add_connection(g_messenger);
    g_messenger->set_dispatcher(shared_from_this());

    // add the ticker, this wakes the system up once in a while so
    // we can gather statistics at a given interval
    //
    g_tick_timer.reset(new tick_timer(instance(), f_statistics_frequency));
    g_communicator->add_connection(g_tick_timer);

    // start runner thread
    //
    f_worker = std::make_shared<sitter_worker>(shared_from_this());
    f_worker_thread = std::make_shared<cppthread::thread>("worker", f_worker);

    // now start the run() loop
    //
    g_communicator->run();

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
    return g_messenger->send_message(message, cache);
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


/** \brief Initialize the sitter server parameters.
 *
 * This function gets the parameters from the sitter configuration file
 * and convert them for use by the server implementation.
 *
 * If a parameter is not valid, the function calls exit(1) so the server
 * does not do anything.
 *
 * \return true if the initialization succeeds, false if any parameter is
 * invalid.
 */
bool server::init_parameters()
{
    // Time Frequency (how often we gather the stats)
    {
        f_statistics_frequency = f_opts.get_long("statistics-frequency");
        if(f_statistics_frequency < 0)
        {
            SNAP_LOG_FATAL
                << "statistic frequency ("
                << f_statistics_frequency
                << ") cannot be a negative number."
                << SNAP_LOG_SEND;
            return false;
        }
        if(f_statistics_frequency < 60)
        {
            // minimum is 1 minute
            f_statistics_frequency = 60;
        }
    }

    // Time Period (how many stats we keep in the db)
    {
        f_statistics_period = f_opts.get_long("statistics-period");
        if(f_statistics_period < 0)
        {
            SNAP_LOG_FATAL
                << "statistic period ("
                << f_statistics_period
                << ") cannot be a negative number."
                << SNAP_LOG_SEND;
            return false;
        }
        if(f_statistics_period < 3600)
        {
            // minimum is 1 hour
            //
            f_statistics_period = 3600;
        }
        // round up to the hour, but keep it in seconds
        //
        f_statistics_period = (f_statistics_period + 3599) / 3600 * 3600;
    }

    // Time To Live (TTL, used to make sure we do not overcrowd the database)
    {
        std::string const statistics_ttl(f_opts.get_string("statistics-ttl"));
        if(statistics_ttl == "off")
        {
            f_statistics_ttl = 0;
        }
        else if(statistics_ttl == "use-period")
        {
            f_statistics_ttl = f_statistics_period;
        }
        else
        {
            if(!advgetopt::validator_integer::convert_string(
                      statistics_ttl
                    , f_statistics_ttl))
            {
                SNAP_LOG_FATAL
                    << "statistic ttl \""
                    << statistics_ttl
                    << "\" is not a valid number."
                    << SNAP_LOG_SEND;
                return false;
            }
            if(f_statistics_ttl < 0)
            {
                SNAP_LOG_FATAL
                    << "statistic ttl ("
                    << statistics_ttl
                    << ") cannot be a negative number."
                    << SNAP_LOG_SEND;
                return false;
            }
            if(f_statistics_ttl != 0
            && f_statistics_ttl < 3600)
            {
                // minimum is 1 hour
                //
                f_statistics_ttl = 3600;
            }
        }
    }

    // Amount of time before we start sending reports by email
    {
        f_error_report_settle_time = f_opts.get_long("error-report-settle-time");

        if(f_error_report_settle_time < 0)
        {
            SNAP_LOG_FATAL
                << "error report settle time ("
                << f_error_report_settle_time
                << ") cannot be a negative number."
                << SNAP_LOG_SEND;
            return false;
        }
        if(f_error_report_settle_time < 60)
        {
            // minimum is 1 minute
            //
            f_error_report_settle_time = 60;
        }
        // TBD: should we have a maximum like 1h?
    }

    // Low priority and span
    {
        std::string const low_priority(f_opts.get_string("error-report-low-priority"));
        if(!low_priority.empty())
        {
            advgetopt::string_list_t prio_span;
            advgetopt::split_string(low_priority, prio_span, {","});
            if(prio_span.size() > 2)
            {
                SNAP_LOG_FATAL
                    << "error report low priority \""
                    << low_priority
                    << "\" is expect to have two numbers separated by one comma. The second number is optional."
                    << SNAP_LOG_SEND;
                return false;
            }

            if(!advgetopt::validator_integer::convert_string(
                          prio_span[0]
                        , f_error_report_low_priority))
            {
                SNAP_LOG_FATAL
                    << "error report low priority \""
                    << low_priority
                    << "\" is not a valid number."
                    << SNAP_LOG_SEND;
                return false;
            }
            if(f_error_report_low_priority < 1)
            {
                SNAP_LOG_FATAL
                    << "error report low priority ("
                    << low_priority
                    << ") cannot be negative or null."
                    << SNAP_LOG_SEND;
                return false;
            }
            if(f_error_report_low_priority > 50)
            {
                // maximum is 50
                //
                f_error_report_low_priority = 50;
            }

            if(prio_span.size() == 2
            && !prio_span[1].empty())
            {
                if(!advgetopt::validator_integer::convert_string(
                              prio_span[1]
                            , f_error_report_low_span))
                {
                    SNAP_LOG_FATAL
                        << "error report low span \""
                        << low_priority
                        << "\" is not a valid number."
                        << SNAP_LOG_SEND;
                    return false;
                }
                if(f_error_report_low_span < 0)
                {
                    SNAP_LOG_FATAL
                        << "error report low span ("
                        << low_priority
                        << ") cannot be negative or null."
                        << SNAP_LOG_SEND;
                    return false;
                }
                if(f_error_report_low_span < 86400)
                {
                    // minimum is one day
                    //
                    f_error_report_low_span = 86400;
                }
            }
        }
    }

    // Medium priority and span
    {
        std::string const medium_priority(f_opts.get_string("error-report-medium-priority"));
        if(!medium_priority.empty())
        {
            advgetopt::string_list_t prio_span;
            advgetopt::split_string(medium_priority, prio_span, {","});
            if(prio_span.size() > 2)
            {
                SNAP_LOG_FATAL
                    << "error report medium priority \""
                    << medium_priority
                    << "\" is expect to have two numbers separated by one comma. The second number is optional."
                    << SNAP_LOG_SEND;
                return false;
            }

            if(!advgetopt::validator_integer::convert_string(
                          prio_span[0]
                        , f_error_report_medium_priority))
            {
                SNAP_LOG_FATAL
                    << "error report medium priority \""
                    << medium_priority
                    << "\" is not a valid number."
                    << SNAP_LOG_SEND;
                return false;
            }
            if(f_error_report_medium_priority < 1)
            {
                SNAP_LOG_FATAL
                    << "error report medium priority ("
                    << medium_priority
                    << ") cannot be negative or null."
                    << SNAP_LOG_SEND;
                return false;
            }

            if(prio_span.size() == 2
            && !prio_span[1].empty())
            {
                if(!advgetopt::validator_integer::convert_string(
                              prio_span[1]
                            , f_error_report_medium_span))
                {
                    SNAP_LOG_FATAL
                        << "error report medium span \""
                        << medium_priority
                        << "\" is not a valid number."
                        << SNAP_LOG_SEND;
                    return false;
                }
                if(f_error_report_medium_span < 0)
                {
                    SNAP_LOG_FATAL
                        << "error report medium span ("
                        << medium_priority
                        << ") cannot be negative or null."
                        << SNAP_LOG_SEND;
                    return false;
                }
                if(f_error_report_medium_span < 3600)
                {
                    // minimum is one hour
                    //
                    f_error_report_medium_span = 3600;
                }
            }
        }
    }

    // Critical priority and span
    {
        std::string const critical_priority(f_opts.get_string("error-report-critical-priority"));
        if(!critical_priority.empty())
        {
            advgetopt::string_list_t prio_span;
            advgetopt::split_string(critical_priority, prio_span, {","});
            if(prio_span.size() > 2)
            {
                SNAP_LOG_FATAL
                    << "error report critical priority \""
                    << critical_priority
                    << "\" is expect to have two numbers separated by one comma. The second number is optional."
                    << SNAP_LOG_SEND;
                return false;
            }

            if(!advgetopt::validator_integer::convert_string(
                          prio_span[0]
                        , f_error_report_critical_priority))
            {
                SNAP_LOG_FATAL
                    << "error report critical priority \""
                    << critical_priority
                    << "\" is not a valid number."
                    << SNAP_LOG_SEND;
                return false;
            }
            if(f_error_report_critical_priority < 1)
            {
                SNAP_LOG_FATAL
                    << "error report critical priority ("
                    << critical_priority
                    << ") cannot be negative or null."
                    << SNAP_LOG_SEND;
                return false;
            }
            if(f_error_report_critical_priority > 100)
            {
                // TBD: should we just clamp silently instead of a fatal error?
                //
                SNAP_LOG_FATAL
                    << "error report critical priority ("
                    << critical_priority
                    << ") cannot be larger than 100."
                    << SNAP_LOG_SEND;
                return false;
            }

            if(prio_span.size() == 2
            && !prio_span[1].empty())
            {
                if(!advgetopt::validator_integer::convert_string(
                              prio_span[1]
                            , f_error_report_critical_span))
                {
                    SNAP_LOG_FATAL
                        << "error report critical span \""
                        << critical_priority
                        << "\" is not a valid number."
                        << SNAP_LOG_SEND;
                    return false;
                }
                if(f_error_report_critical_span < 0)
                {
                    SNAP_LOG_FATAL
                        << "error report critical span ("
                        << critical_priority
                        << ") cannot be negative or null."
                        << SNAP_LOG_SEND;
                    return false;
                }
                if(f_error_report_critical_span < 300)
                {
                    // minimum is five minutes
                    //
                    f_error_report_critical_span = 300;
                }
            }
        }
    }

    // now that all the priority & span numbers are defined we can verify
    // that they are properly sorted
    //
    if(f_error_report_medium_priority < f_error_report_low_priority)
    {
        SNAP_LOG_FATAL
            << "error report medium priority ("
            << f_error_report_medium_priority
            << ") cannot be less than the low priority ("
            << f_error_report_low_priority
            << ")."
            << SNAP_LOG_SEND;
        return false;
    }
    if(f_error_report_critical_priority < f_error_report_medium_priority)
    {
        SNAP_LOG_FATAL
            << "error report critical priority ("
            << f_error_report_critical_priority
            << ") cannot be less than the medium priority ("
            << f_error_report_medium_priority
            << ")."
            << SNAP_LOG_SEND;
        return false;
    }

    if(f_error_report_medium_span > f_error_report_low_span)
    {
        SNAP_LOG_FATAL
            << "error report medium span ("
            << f_error_report_medium_span
            << ") cannot be more than the low span ("
            << f_error_report_low_span
            << ")."
            << SNAP_LOG_SEND;
        return false;
    }
    if(f_error_report_critical_span > f_error_report_medium_span)
    {
        SNAP_LOG_FATAL
            << "error report critical span ("
            << f_error_report_critical_span
            << ") cannot be more than the medium span ("
            << f_error_report_medium_span
            << ")."
            << SNAP_LOG_SEND;
        return false;
    }

    return true;
}


void server::msg_rusage(ed::message & message)
{
    record_usage(message);
}


void server::ready(ed::message & message)
{
    snapdev::NOT_USED(message);

    // TBD: should we wait on this signal before we start the g_tick_timer?
    //      since we do not need the snap communicator, probably not useful
    //      (however, we like to have Cassandra and we know Cassandra is
    //      ready only after a we got the CASSANDRAREADY anyway...)
    //

    // request snapdbproxy to send us a status signal about
    // Cassandra, after that one call, we will receive the
    // changes in status just because we understand them.
    //
    ed::message isdbready_message;
    isdbready_message.set_command("CASSANDRASTATUS");
    isdbready_message.set_service("snapdbproxy");
    g_messenger->send_message(isdbready_message);
}


void server::msg_reload_config(ed::message & message)
{
    snapdev::NOT_USED(message);

    f_force_restart = true;
    stop(false);
}


int64_t server::get_error_report_settle_time() const
{
    return f_error_report_settle_time;
}


int64_t server::get_error_report_low_priority() const
{
    return f_error_report_low_priority;
}


int64_t server::get_error_report_low_span() const
{
    return f_error_report_low_span;
}


int64_t server::get_error_report_medium_priority() const
{
    return f_error_report_medium_priority;
}


int64_t server::get_error_report_medium_span() const
{
    return f_error_report_medium_span;
}


int64_t server::get_error_report_critical_priority() const
{
    return f_error_report_critical_priority;
}


int64_t server::get_error_report_critical_span() const
{
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



bool server::output_process(
      std::string const & plugin_name
    , as2js::JSON::JSONValueRef & json
    , cppprocess::process_info::pointer_t info
    , std::string const & process_name
    , int priority)
{
    as2js::JSON::JSONValueRef process(json["process"][-1]);
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

    if(g_messenger != nullptr)
    {
        if(quitting || !g_messenger->is_connected())
        {
            // turn off that connection now, we cannot UNREGISTER since
            // we are not connected to communicatord
            //
            g_communicator->remove_connection(g_messenger);
            g_messenger.reset();
        }
        else
        {
            g_messenger->mark_done();

            // if not communicatord is not quitting, send an UNREGISTER
            //
            g_messenger->unregister_service();
            //ed::message unregister;
            //unregister.set_command("UNREGISTER");
            //unregister.add_parameter("service", "sitter");
            //g_messenger->send_message(unregister);
        }
    }

    g_communicator->remove_connection(g_interrupt);
    g_communicator->remove_connection(g_tick_timer);
}



void server::set_communicatord_connected(bool status)
{
    if(status)
    {
        f_communicatord_connected = time(nullptr);
    }
    else
    {
        f_communicatord_disconnected = time(nullptr);
    }
}



bool server::get_communicatord_is_connected() const
{
    return f_communicatord_disconnected < f_communicatord_connected;
}



time_t server::get_communicatord_connected_on() const
{
    return f_communicatord_connected;
}



time_t server::get_communicatord_disconnected_on() const
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
 * the specified filename.
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

    as2js::JSON json;

    as2js::JSON::JSONValueRef e(json["rusage"]);

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

    std::string const data(json.get_value()->to_string().to_utf8());

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
 * \param[in] doc  The DOM document where the \<error> tag is created.
 * \param[in] plugin_name  The name of the plugin generating this error.
 * \param[in] message  The error message. This is free form. It can't include
 *            tags (\< and \> will be inserted as \&lt; and \&gt;.)
 * \param[in] priority  The priority of this error. The higher the greater the
 *            priority and the sooner the error will be sent to the
 *            administrator.
 */
void server::append_error(
      as2js::JSON::JSONValueRef & json_ref
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

    // get the error array
    //
    as2js::JSON::JSONValueRef error(json_ref["error"]);

    // create a new item in the array (at the end)
    //
    as2js::JSON::JSONValueRef err(json_ref[-1]);
    err[as2js::String("plugin_name")] = plugin_name;
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
