// Snap Websites Server -- CPU watchdog: record CPU usage over time
// Copyright (c) 2013-2019  Made to Order Software Corp.  All Rights Reserved
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
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#pragma once

// self
//
#include    "sitter/sitter.h"



namespace sitter
{
namespace cpu
{



class cpu
    : public cppthread::plugin
{
public:
                        cpu();
                        cpu(cpu const & rhs) = delete;
    virtual             ~cpu() override;

    cpu &               operator = (cpu const & rhs) = delete;

    static cppthread::plugin::pointer_t
                        instance();

    // plugins::plugin implementation
    virtual void        bootstrap(void * s) override;

    // server signal
    void                on_process_watch(as2js::JSON::JSONValueRef & json);

private:
    server *            f_server = nullptr;
};


} // namespace cpu
} // namespace sitter
// vim: ts=4 sw=4 et
