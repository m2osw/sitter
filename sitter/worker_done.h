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


// eventdispatcher
//
//#include    <eventdispatcher/communicator.h>
#include    <eventdispatcher/thread_done_signal.h>
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
 * \brief This file represents the interrupt signal handler.
 *
 * The sitter captures the Ctrl-C event in order to cleanly disconnect
 * and quit. This is the declaration of that interrupt class.
 */




namespace sitter
{



class server;

class worker_done
    : public ed::thread_done_signal
{
public:
    typedef std::shared_ptr<worker_done>     pointer_t;

                        worker_done(server * s);
                        worker_done(worker_done const & rhs) = delete;
    virtual             ~worker_done() override;
    worker_done &       operator = (worker_done const & rhs) = delete;

    // ed::connection implementation
    virtual void        process_read() override;

private:
    server *            f_server = nullptr;
};



} // namespace sitter
// vim: ts=4 sw=4 et
