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
#include    "sitter/interrupt.h"

//#include    "sitter/names.h"
#include    "sitter/sitter.h"
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
#include    <signal.h>


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



/** \brief The interrupt initialization.
 *
 * The interrupt uses the signalfd() function to obtain a way to listen on
 * incoming Unix signals.
 *
 * Specifically, it listens on the SIGINT signal, which is the equivalent
 * to the Ctrl-C.
 *
 * \param[in] s  The server we are listening for.
 */
interrupt::interrupt(server * s)
    : signal(SIGINT)
    , f_server(s)
{
    unblock_signal_on_destruction();
    set_name("interrupt");
}


interrupt::~interrupt()
{
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



} // namespace sitter
// vim: ts=4 sw=4 et
