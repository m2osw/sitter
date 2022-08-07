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
#pragma once

//// self
////
//#include    "sitter/sitter.h"
//
//#include    "sitter/exception.h"
//#include    "sitter/names.h"
//#include    "sitter/version.h"
//
//
//// libmimemail
////
//#include    <libmimemail/email.h>
//
//
//// snaplogger
////
//#include    <snaplogger/logger.h>
//#include    <snaplogger/message.h>
//#include    <snaplogger/options.h>
//
//
//// advgetopt
////
//#include    <advgetopt/conf_file.h>
//#include    <advgetopt/exception.h>
//#include    <advgetopt/validator_integer.h>
//
//
//// libaddr
////
//#include    <libaddr/addr_parser.h>


// fluid-settings
//
#include    <fluid-settings/fluid_settings_connection.h>


// eventdispatcher
//
//#include    <eventdispatcher/communicator.h>
//#include    <eventdispatcher/signal.h>
//#include    <eventdispatcher/tcp_client_permanent_message_connection.h>


//// snapdev
////
//#include    <snapdev/file_contents.h>
//#include    <snapdev/gethostname.h>
//#include    <snapdev/glob_to_list.h>
//#include    <snapdev/mkdir_p.h>
//#include    <snapdev/not_reached.h>
//#include    <snapdev/not_used.h>
//#include    <snapdev/string_replace_many.h>
//
//
//// C++
////
//#include    <algorithm>
//#include    <fstream>
//
//
//// C
////
//#include    <sys/wait.h>


/** \file
 * \brief This file represents the Sitter messenger.
 *
 * The sitter communicates to the communicator as well as the fluid-settings
 * service. This is all done via this class.
 */




namespace sitter
{



class server;

class messenger
    : public fluid_settings::fluid_settings_connection
{
public:
    typedef std::shared_ptr<messenger>    pointer_t;

                                messenger(server * s, advgetopt::getopt & opts);
                                messenger(messenger const & rhs) = delete;
    virtual                     ~messenger() override {}
    messenger &                 operator = (messenger const & rhs) = delete;

    void                        finish_initialization(ed::dispatcher::pointer_t dispatcher);
    void                        fluid_settings_changed(
                                      fluid_settings::fluid_settings_status_t status
                                    , std::string const & name
                                    , std::string const & value);

private:
    // this is owned by a server object so no need for a smart pointer
    server *                    f_server = nullptr;
};



} // namespace sitter
// vim: ts=4 sw=4 et
