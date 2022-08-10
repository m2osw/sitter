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

// advgetopt
//
#include    <advgetopt/advgetopt.h>


// eventdispatcher
//
#include    <eventdispatcher/timer.h>



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
