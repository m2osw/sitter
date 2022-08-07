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
#include    "sitter/worker_done.h"

//#include    "sitter/names.h"
#include    "sitter/sitter.h"
//#include    "sitter/version.h"


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
//
//
//// eventdispatcher
////
//#include    <eventdispatcher/communicator.h>
//#include    <eventdispatcher/signal.h>
//#include    <eventdispatcher/tcp_client_permanent_message_connection.h>
//
//
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


// C
//
//#include    <sys/wait.h>
//#include    <signal.h>


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




namespace sitter
{



/** \brief The worker_done initialization.
 *
 * The worker_done uses the eventdispatcher worker_done_signal connection.
 * That object allows us to receive a signal when a thread ends cleanly
 * or dies.
 *
 * \param[in] s  The server we are listening for.
 */
worker_done::worker_done(server * s)
    : f_server(s)
{
    set_name("worker_done");
}


worker_done::~worker_done()
{
}


/** \brief Call the stop function of the snaplock object.
 *
 * When this function is called, the signal was received and thus we are
 * asked to quit as soon as possible.
 */
void worker_done::process_read()
{
    // call the default function
    //
    thread_done_signal::process_read();

    // we simulate the STOP, so pass 'false' (i.e. not quitting)
    //
    f_server->stop(false);
}



} // namespace sitter
// vim: ts=4 sw=4 et