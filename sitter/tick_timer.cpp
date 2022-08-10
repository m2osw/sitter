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
#include    "sitter/tick_timer.h"

#include    "sitter/sitter.h"


// snaplogger
//
#include    <snaplogger/message.h>


// last include
//
#include    <snapdev/poison.h>





/** \file
 * \brief This file implements the Sitter tick timer.
 *
 * The sitter needs to wake up once in a while to run all the code in
 * its plugins to verify the current health of your system. The tick
 * is what wakes up the Sitter for this purpose.
 */



namespace sitter
{




/** \class tick_timer
 * \brief The timer to produce ticks once every minute.
 *
 * This timer is the one used to know when to gather data again.
 *
 * By default the interval is set to one minute, although it is possible
 * to change that amount in the configuration file.
 */




/** \brief Initializes the timer with a pointer to the snap backend.
 *
 * The constructor saves the pointer of the snap_backend object so
 * it can later be used when the process times out.
 *
 * The timer is setup to trigger after one minute. After that, it will
 * make use of the server::get_statistics_frequency() function to determine
 * the amount of time to wait between attempts.
 *
 * This is what starts the backend process checking things that the sitter
 * is expected to check.
 *
 * \param[in] s  A pointer to the server object.
 */
tick_timer::tick_timer(server * s)
    : timer(60'000'000LL)  // wait 1 minute before the first attempt
    , f_server(s)
{
    set_name("tick_timer");
    set_enable(false);
}


tick_timer::~tick_timer()
{
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

    // the timeout delay may change through fluid-settings
    //
    set_timeout_delay(f_server->get_statistics_frequency() * 1'000'000LL);
}



} // namespace sitter
// vim: ts=4 sw=4 et
