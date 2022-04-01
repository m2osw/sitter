// Snap Websites Server -- watchdog processes
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
#include    <sitter/sitter.h>


// serverplugins
//
#include    <serverplugins/plugin.h>



namespace sitter
{
namespace processes
{



class processes
    : public serverplugins::plugin
{
public:
    SERVERPLUGINS_DEFAULTS(processes);

    // serverplugins::plugin implementation
    virtual void        bootstrap() override;

    // server signals
    void                on_process_watch(as2js::JSON::JSONValueRef & json);

private:
};

} // namespace processes
} // namespace sitter
// vim: ts=4 sw=4 et
