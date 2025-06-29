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
#pragma once


// eventdispatcher
//
#include    <eventdispatcher/thread_done_signal.h>



/** \file
 * \brief This declares an object that captures a signal from the worker.
 *
 * This object is used to know when the worker dies. If the worker dies
 * inadvertendly, then we want the whole service to exit. This signal
 * helps us in that part.
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
