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
#include    "sitter/sitter_worker.h"

#include    "sitter/names.h"
#include    "sitter/sitter.h"
#include    "sitter/version.h"


// cppthread
//
#include    <cppthread/guard.h>


// libmimemail
//
#include    <libmimemail/email.h>


// snaplogger
//
#include    <snaplogger/logger.h>


// advgetopt
//
#include    <advgetopt/validator_integer.h>


// snapdev
//
#include    <snapdev/file_contents.h>
#include    <snapdev/gethostname.h>
#include    <snapdev/trim_string.h>


// last include
//
#include    <snapdev/poison.h>





/** \file
 * \brief This file is the implementation of the sitter_worker.
 *
 * The sitter_worker class handles the loading of the plugins and then
 * running of the statistics gathering.
 */



namespace sitter
{




sitter_worker::sitter_worker(
          server::pointer_t s
        , worker_done::pointer_t done)
    : runner("sitter-worker")
    , f_server(s)
    , f_worker_done(done)
{
}


sitter_worker::~sitter_worker()
{
}


void sitter_worker::enter()
{
    runner::enter();

    load_plugins();
}


void sitter_worker::run()
{
    loop();
}


void sitter_worker::leave(cppthread::leave_status_t status)
{
    runner::leave(status);

    f_worker_done->thread_done();
}


void sitter_worker::tick()
{
    cppthread::guard lock(f_mutex);

    ++f_ticks;

    f_mutex.signal();
}


void sitter_worker::wakeup()
{
    cppthread::guard lock(f_mutex);

    // make sure f_ticks is not 0
    //
    f_ticks = 1;

    f_mutex.signal();
}


void sitter_worker::load_plugins()
{
    serverplugins::paths paths;
    std::string const plugins_path(f_server->get_server_parameter("plugins-path"));
    paths.add(plugins_path);

    serverplugins::names names(paths);
    std::string plugins(snapdev::trim_string(f_server->get_server_parameter("plugins")));
    if(plugins.empty() || plugins == "*")
    {
        names.find_plugins("sitter_");
    }
    else
    {
        // user specified list
        //
        advgetopt::string_list_t plugin_names;
        advgetopt::split_string(plugins, plugin_names, {","});
        plugins.clear();
        for(auto & p : plugin_names)
        {
            if(p.substr(0, 7) != "sitter_")
            {
                p = "sitter_" + p;
            }
            if(!plugins.empty())
            {
                plugins += ',';
            }
            plugins += p;
        }
        names.add(plugins);
    }

    f_plugins = std::make_shared<serverplugins::collection>(names);
    f_plugins->load_plugins(f_server);
}


void sitter_worker::loop()
{
    while(continue_running())
    {
        wait_next_tick();
        if(!continue_running())
        {
            return;
        }
        run_plugins();
    }
}


void sitter_worker::wait_next_tick()
{
    cppthread::guard lock(f_mutex);

    while(continue_running())
    {
        if(f_ticks != 0)
        {
            f_server->set_ticks(f_ticks);
            f_ticks = 0;
            return;
        }

        f_mutex.wait();
    }
}

void sitter_worker::run_plugins()
{
    as2js::json json;

    as2js::json::json_value_ref root(json["sitter"]);

    time_t const start_date(time(nullptr));
    root["start_date"] = start_date;

    f_server->clear_errors();

    // while running the plugins we want to have a severity of WARNING
    // because otherwise we get a ton of messages all the time
    //
    // TODO: find a way to only affect this thread?!
    //
    {
        // TODO: let user define that minimum level
        //
        snaplogger::override_lowest_severity_level save_log_level(snaplogger::severity_t::SEVERITY_WARNING);
        f_server->process_watch(root);
    }

    time_t const end_date(time(nullptr));
    root["end_date"] = end_date;

    as2js::json::json_value::object_t obj(root);
    if(obj.size() <= 2)
    {
        static bool err_once(true);
        if(err_once)
        {
            err_once = false;
            SNAP_LOG_ERROR
                << "sitter_worker::run_plugins() generated a completely empty result. This can happen if you did not define any sitter plugins."
                << SNAP_LOG_SEND;
        }
        return;
    }

    // if user specified a data path, save data to a file
    //
    std::string const data_path(f_server->get_server_parameter(g_name_sitter_data_path));
    if(!data_path.empty())
    {
        std::int64_t const date(((start_date / 60LL) * 60LL) % f_server->get_statistics_period());
        std::string const filename(data_path + '/' + std::to_string(date) + ".json");
        snapdev::file_contents output(filename);
        output.contents(json.get_value()->to_string());
        output.write_all();
    }

    int const error_count(f_server->get_error_count());
    if(error_count > 0)
    {
        std::int64_t const diff(end_date - start_date);
        if(diff >= f_server->get_error_report_settle_time())
        {
            report_error(json, start_date);
        }
    }
}


void sitter_worker::report_error(as2js::json & json, time_t start_date)
{
    // how often to send an email depends on the priority
    // and the span parameters
    //
    // note that too often on a large cluster and you'll
    // die under the pressure! (some even call it spam)
    // so we limit the emails quite a bit by default...
    // admins can check the status any time from the
    // server side in snapmanager anyway and also the
    // priorities and span parameters can be changed
    // in the configuration file (search for
    // `error_report_` parameters in sitter.conf)
    //
    // note that the span lasts across restarts of the
    // service
    //
    // the defaults at this time are:
    //
    // +----------+----------+--------+
    // | name     | priority | span   |
    // +----------+----------+--------+
    // | low      |       10 | 1 week |
    // | medium   |       50 | 3 days |
    // | critical |       90 | 1 day  |
    // +----------+----------+--------+
    //
    int const max_error_priority(f_server->get_max_error_priority());
    if(max_error_priority < f_server->get_error_report_low_priority())
    {
        // too low a priority, ignore the errors altogether
        //
        return;
    }

    int64_t span(f_server->get_error_report_low_span());
    if(max_error_priority >= f_server->get_error_report_critical_priority())
    {
        span = f_server->get_error_report_critical_span();
    }
    else if(max_error_priority >= f_server->get_error_report_medium_priority())
    {
        span = f_server->get_error_report_medium_span();
    }

    // use a file in the cache area since we are likely
    // to regenerate it often or just ignore it for a
    // while (and if ignored for a while it could as
    // well be deleted)
    //
    std::string const last_email_filename(f_server->get_cache_path("last_email_time.txt"));

    time_t const now(time(nullptr));
    snapdev::file_contents last_email_time(last_email_filename);
    if(last_email_time.read_all())
    {
        // when the file exists we want to read it
        // first and determine whether 'span' has
        // passed, if so, we write 'now' in the file
        // and send the email
        //
        std::int64_t last_mail_date(0);
        if(advgetopt::validator_integer::convert_string(
                  last_email_time.contents()
                , last_mail_date))
        {
            if(now - last_mail_date < span)
            {
                // span has not yet elapsed, keep
                // the file as is and don't send
                // the email
                //
                return;
            }
        }
    }

    // first save the time when we are sending the email
    //
    last_email_time.contents(std::to_string(now));
    if(!last_email_time.write_all())
    {
        SNAP_LOG_NOTICE
            << "could not save last email time to \""
            << last_email_time.filename()
            << "\"."
            << SNAP_LOG_SEND;
    }

    // get the emails where to send the data
    // if not available, it "breaks" the process
    //
    std::string const from_email(f_server->get_server_parameter(g_name_sitter_from_email));
    std::string const administrator_email(f_server->get_server_parameter(g_name_sitter_administrator_email));
    if(from_email.empty()
    || administrator_email.empty())
    {
        return;
    }

    // create the email and add a few headers
    //
    libmimemail::email e;
    e.set_from(from_email);
    e.set_to(administrator_email);
    e.set_priority(libmimemail::priority_t::EMAIL_PRIORITY_URGENT);

    std::string subject("sitter: found ");
    subject += std::to_string(f_server->get_error_count());
    subject += " error";
    subject += f_server->get_error_count() == 1 ? "" : "s";
    subject += " on ";
    subject += snapdev::gethostname();
    e.set_subject(subject);

    e.add_header("X-Sitter-Version", SITTER_VERSION_STRING);

    // prevent blacklisting
    // (since we won't run the `sendmail` plugin validation, it's not necessary)
    //e.add_parameter(sendmail::get_name(sendmail::name_t::SNAP_NAME_SENDMAIL_BYPASS_BLACKLIST), "true");

    // TODO: transform JSON to "neat" (useful) HTML
    //
    libmimemail::attachment html;
    std::string const data(json.get_value()->to_string());
    html.quoted_printable_encode_and_set_data("<p>" + data + "</p>", "text/html");
    e.set_body_attachment(html);

    // also add the JSON as an attachment
    //
    libmimemail::attachment a;
    a.quoted_printable_encode_and_set_data(data, "application/json");
    a.set_content_disposition("sitter.json");
    a.add_header("X-Start-Date", std::to_string(start_date));
    e.add_attachment(a);

    // finally send email
    //
    e.send();
}


} // namespace snap
// vim: ts=4 sw=4 et
