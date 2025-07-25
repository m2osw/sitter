// Copyright (c) 2011-2025  Made to Order Software Corp.  All Rights Reserved
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


// sitter
//
#include    <sitter/sitter.h>


// advgetopt
//
#include    <advgetopt/exception.h>


// snaplogger
//
#include    <snaplogger/message.h>


// eventdispatcher
//
#include    <eventdispatcher/signal_handler.h>


// libexcept
//
#include    <libexcept/exception.h>
#include    <libexcept/stack_trace.h>


// snapdev
//
#include    <snapdev/not_reached.h>


// last include
//
#include    <snapdev/poison.h>



int main(int argc, char * argv[])
{
    ed::signal_handler::create_instance();
    libexcept::collect_stack_trace();

    try
    {
        sitter::server::pointer_t s(std::make_shared<sitter::server>(argc, argv));
        s->set_instance(s);
        return s->run();
    }
    catch(advgetopt::getopt_exit const & e)
    {
        return e.code();
    }
    catch(libexcept::exception_t const & e)
    {
        SNAP_LOG_FATAL
            << "sitter: libexcept::exception caught: "
            << e.what()
            << SNAP_LOG_SEND;
    }
    catch(std::exception const & e)
    {
        SNAP_LOG_FATAL
            << "sitter: std::exception caught: "
            << e.what()
            << SNAP_LOG_SEND;
    }
    catch(...)
    {
        SNAP_LOG_FATAL
            << "sitter: unknown exception caught!"
            << SNAP_LOG_SEND;
    }

    return 1;
}



// vim: ts=4 sw=4 et
