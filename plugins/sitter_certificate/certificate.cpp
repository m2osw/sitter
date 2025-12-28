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


// self
//
#include    "certificate.h"


// eventdispatcher
//
#include    <eventdispatcher/certificate.h>


//// libexcept
////
//#include    <libexcept/exception.h>


// snaplogger
//
#include    <snaplogger/message.h>


// advgetopt
//
//#include    <advgetopt/utils.h>
#include    <advgetopt/validator_integer.h>


//// serverplugins
////
//#include    <serverplugins/collection.h>


// snapdev
//
#include    <snapdev/join_strings.h>
#include    <snapdev/glob_to_list.h>
//#include    <snapdev/not_used.h>


//// C++
////
//#include    <thread>


// last include
//
#include    <snapdev/poison.h>



namespace sitter
{
namespace certificate
{

SERVERPLUGINS_START(certificate)
    , ::serverplugins::description(
            "Check for the /run/certificate-required flag and raise one of our flags if set.")
    , ::serverplugins::dependency("server")
    , ::serverplugins::help_uri("https://snapwebsites.org/help")
    , ::serverplugins::categorization_tag("os")
SERVERPLUGINS_END(certificate)



namespace
{



constexpr char const * const    g_default_certificate_path = "/etc/sitter/certificates";
constexpr char const * const    g_domain = "domain"; // TODO: probably move this one to the names.an instead
time_t                          g_last_delay_error = 0;



} // no name namespace



/** \brief Initialize the certificate plugin.
 *
 * This function terminates the initialization of the certificate plugin
 * by registering for different events.
 */
void certificate::bootstrap()
{
    SERVERPLUGINS_LISTEN(certificate, server, process_watch, std::placeholders::_1);
}


/** \brief Process the certificate plugin.
 *
 * This function checks whether the "/run/certificate-required" flag is set.
 * If so, then we generate an error about the state.
 *
 * The priority changes depending on how long it has been in that
 * state.
 *
 * \param[in] json  The document where the results are collected.
 */
void certificate::on_process_watch(as2js::json::json_value_ref & json)
{
    SNAP_LOG_DEBUG
        << "certificate::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    time_t const now(time(nullptr));
    as2js::json::json_value_ref e(json["certificate"]);

    // we reload the set of delays each time since the user may change
    // them on the fly through fluid-settings
    //
    parse_delays();

    // find the list of domains from which certificates need to be checked
    //
    sitter::server::pointer_t server(plugins()->get_server<sitter::server>());
    std::string path(server->get_server_parameter("certificate_path"));
    if(path.empty())
    {
        path = g_default_certificate_path;
    }
    snapdev::glob_to_list<std::set<std::string>> glob;
    if(glob.read_path<
             snapdev::glob_to_list_flag_t::GLOB_FLAG_IGNORE_ERRORS,
             snapdev::glob_to_list_flag_t::GLOB_FLAG_EMPTY>(path + "/[0-9]0-9]-*.conf"))
    {
        time_t const today(now / 86400);
        for(auto const & f : glob)
        {
            advgetopt::conf_file_setup setup(f);
            advgetopt::conf_file::pointer_t config(advgetopt::conf_file::get_conf_file(setup));
            if(config != nullptr
            && config->has_parameter(g_domain))
            {
                std::string const domain(config->get_parameter(g_domain));
                e["domain"] = domain;

                ed::certificate cert;
                if(cert.load_from_domain(domain))
                {
                    snapdev::timespec_ex const not_after(cert.get_not_after());
                    if(not_after)
                    {
                        time_t const not_after_day(not_after.tv_sec / 86400);
                        time_t const diff(not_after_day - today);
                        if(diff <= 0)
                        {
                            plugins()->get_server<sitter::server>()->append_error(
                                  json
                                , "certificate"
                                , "Certificate for domain \""
                                    + domain
                                    + "\" has expired on "
                                    + not_after.to_string()
                                    + "."
                                , 100);
                        }
                        else
                        {
                            for(auto const d : f_delays_n_priorities)
                            {
                                if(diff <= d.first)
                                {
                                    plugins()->get_server<sitter::server>()->append_error(
                                          json
                                        , "certificate"
                                        , "Certificate for domain \""
                                            + domain
                                            + "\" will expire on "
                                            + not_after.to_string()
                                            + " (in "
                                            + std::to_string(diff)
                                            + " day"
                                            + (diff != 1 ? "s" : "")
                                            + ")."
                                        , d.second);
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        plugins()->get_server<sitter::server>()->append_error(
                              json
                            , "certificate"
                            , "Failed getting the certificate notAfter date for domain \""
                                + domain
                                + "\"."
                            , 90);
                    }
                }
                else
                {
                    // TODO: get the error from the load_from_domain()
                    //
                    // we failed accessing the domain, this is a high level
                    // error unless it was just this one time; in other words
                    // quite temporary and thus we don't want to send an error
                    // unless it repeats for a while first;
                    //
                    int report_error(1);
                    auto it(f_access_error.find(domain));
                    if(it != f_access_error.end())
                    {
                        if(now - it->second > 3600 * 5)
                        {
                            report_error = 2;
                        }
                        else
                        {
                            report_error = 0;
                        }
                    }
                    if(report_error > 0)
                    {
                        f_access_error[domain] = now;
                        plugins()->get_server<sitter::server>()->append_error(
                              json
                            , "certificate"
                            , "Failed loading certificate of domain \""
                                + domain
                                + "\"."
                            , report_error > 1 ? 100 : 75);
                    }
                }
            }
        }
    }
}


void certificate::parse_delays()
{
    // the warning delays are defined as a comma separated list of two
    // parameters separated by a slash:
    //
    //    <delay in days>/<priority>, <delay in days>/<priority>, ...
    //
    // the list is expected to be sorted from the highest priority
    // (and thus smallest number of days) to the lowest priority
    //
    f_delays_n_priorities.clear();

    advgetopt::string_list_t invalid_delays;

    std::string warning_delays(plugins()->get_server<sitter::server>()->get_server_parameter("certificate_warning_delays"));
    advgetopt::string_list_t warnings;
    advgetopt::split_string(warning_delays, warnings, {","});
    for(std::size_t idx(0); idx < warnings.size(); ++idx)
    {
        advgetopt::string_list_t delay_priority;
        advgetopt::split_string(warnings[idx], delay_priority, {"/"});
        if(delay_priority.size() != 2)
        {
            invalid_delays.push_back(warnings[idx]);
            continue;
        }

        std::int64_t delay(0);
        if(!advgetopt::validator_integer::convert_string(delay_priority[0], delay))
        {
            invalid_delays.push_back(warnings[idx]);
            continue;
        }
        if(delay <= 0
        || delay > 366 * 10) // allow up to 10 years
        {
            invalid_delays.push_back(warnings[idx]);
            continue;
        }

        std::int64_t priority(0);
        if(!advgetopt::validator_integer::convert_string(delay_priority[1], priority))
        {
            invalid_delays.push_back(warnings[idx]);
            continue;
        }
        if(priority < 0
        || priority > 100)
        {
            invalid_delays.push_back(warnings[idx]);
            continue;
        }

        f_delays_n_priorities[delay] = priority;
    }

    if(!invalid_delays.empty())
    {
        // avoid sending the error over and over again; just once a day
        // is more than sufficient
        //
        time_t const now(time(nullptr));
        if(now - g_last_delay_error >= 86400)
        {
            g_last_delay_error = now;
            SNAP_LOG_ERROR
                << "invalid delays or priorities, delays must be positive up"
                   " to 3660 and priorities must be between 0 and 100; the"
                   " delay and priority must be separated by a slash; multiple"
                   " entries must be separated by commas;"
                   " we found these that we ignored \""
                << snapdev::join_strings(invalid_delays, ", ")
                << "\"."
                << SNAP_LOG_SEND;
        }
    }

    // if all were invalid or the user did not specify his own delay/priority
    // entries, add our defaults
    //
    if(f_delays_n_priorities.empty())
    {
        // 7/100, 14/85, 30/45
        //
        f_delays_n_priorities[ 7] = 100;
        f_delays_n_priorities[14] =  85;
        f_delays_n_priorities[30] =  45;
    }
}



} // namespace reboot
} // namespace sitter
// vim: ts=4 sw=4 et
