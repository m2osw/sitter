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


// advgetopt
//
#include    <advgetopt/advgetopt.h>
//#include    <advgetopt/conf_file.h>
//#include    <advgetopt/exception.h>
//#include    <advgetopt/validator_integer.h>


// eventdispatcher
//
//#include    <eventdispatcher/communicator.h>
#include    <eventdispatcher/timer.h>
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
 * \brief This file declares a timer to receive ticks.
 *
 * The sitter daemon runs a set of commands defined by plugins to determine
 * the health of your system. This happens once per tick. The tick is used
 * only for that purpose.
 *
 * This is considered an internal class.
 */




namespace sitter
{



class server;

class tick_timer
    : public ed::timer
{
public:
    typedef std::shared_ptr<tick_timer>        pointer_t;

                                tick_timer(server * s);
                                tick_timer(tick_timer const & rhs) = delete;
    virtual                     ~tick_timer() override;
    tick_timer &                operator = (tick_timer const & rhs) = delete;

    // ed::timer implementation
    virtual void                process_timeout() override;

private:
    server *                    f_server = nullptr;
};



} // namespace sitter
// vim: ts=4 sw=4 et
