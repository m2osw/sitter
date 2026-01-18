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
#include    "sitter/messenger.h"

#include    "sitter/names.h"
#include    "sitter/sitter.h"


// last include
//
#include    <snapdev/poison.h>





/** \file
 * \brief This file represents the connection to the communicator daemon.
 *
 * The sitter communicates with some other services, especially the
 * communicator and the fluid-settings. This connection is used for
 * that communication. It also listens on LOG_ROTATE messages.
 */



namespace sitter
{





/** \class messenger
 * \brief Handle messages from the communicatord server.
 *
 * This class is an implementation of the TCP client message connection
 * so we can handle incoming messages.
 */




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
 * \param[in] s  A pointer back to the sitter server object.
 * \param[in] opts  A reference to your service advgetopt::getopt object.
 */
messenger::messenger(server * s, advgetopt::getopt & opts)
    : fluid_settings::fluid_settings_connection(opts, "sitter")
    , f_server(s)
{
    set_name("sitter_messenger");

    get_dispatcher()->add_matches({
        ed::define_match(
              ed::Expression(g_name_sitter_cmd_rusage)
            , ed::Callback(std::bind(&server::msg_rusage, f_server, std::placeholders::_1))
        ),
    });
}


void messenger::finish_initialization()
{
    process_fluid_settings_options();
    automatic_watch_initialization();
}


void messenger::fluid_settings_changed(
      fluid_settings::fluid_settings_status_t status
    , std::string const & name
    , std::string const & value)
{
    fluid_settings_connection::fluid_settings_changed(status, name, value);

    switch(status)
    {
    case fluid_settings::fluid_settings_status_t::FLUID_SETTINGS_STATUS_NEW_VALUE:
        // after a SET from another service
        f_server->clear_cache(name);
        break;

    //case fluid_settings::fluid_settings_status_t::FLUID_SETTINGS_STATUS_VALUE:
    //    f_parent->value(name, value, false);
    //    break;

    //case fluid_settings::fluid_settings_status_t::FLUID_SETTINGS_STATUS_DEFAULT:
    //    f_parent->value(name, value, true);
    //    break;

    //case fluid_settings::fluid_settings_status_t::FLUID_SETTINGS_STATUS_UNDEFINED:
    //    break;

    //case fluid_settings::fluid_settings_status_t::FLUID_SETTINGS_STATUS_DELETED:
    //    f_parent->deleted();
    //    break;

    //case fluid_settings::fluid_settings_status_t::FLUID_SETTINGS_STATUS_UPDATED:
    //    f_parent->updated();
    //    break;

    //case fluid_settings::fluid_settings_status_t::FLUID_SETTINGS_STATUS_REGISTERED:
    //    f_parent->registered();
    //    break;

    case fluid_settings::fluid_settings_status_t::FLUID_SETTINGS_STATUS_READY:
        f_server->fluid_ready();
        break;

    //case fluid_settings::fluid_settings_status_t::FLUID_SETTINGS_STATUS_TIMEOUT:
    //    f_parent->timeout();
    //    break;

    //case fluid_settings::fluid_settings_status_t::FLUID_SETTINGS_STATUS_UNAVAILABLE:
    //    f_parent->close();
    //    break;

    default:
        // at this time ignore the other statuses
        break;

    }
}



} // namespace sitter
// vim: ts=4 sw=4 et
