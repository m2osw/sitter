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

// eventdispatcher
//
#include    <eventdispatcher/signal.h>



/** \file
 * \brief This file represents the interrupt signal handler.
 *
 * The sitter captures the Ctrl-C event in order to cleanly disconnect
 * and quit. This is the declaration of that interrupt class.
 */




namespace sitter
{



class server;

class interrupt
    : public ed::signal
{
public:
    typedef std::shared_ptr<interrupt>     pointer_t;

                        interrupt(server * s);
                        interrupt(interrupt const & rhs) = delete;
    virtual             ~interrupt() override;
    interrupt &         operator = (interrupt const & rhs) = delete;

    // ed::connection implementation
    virtual void        process_signal() override;

private:
    server *            f_server = nullptr;
};



} // namespace sitter
// vim: ts=4 sw=4 et
