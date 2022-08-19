// Copyright (c) 2013-2022  Made to Order Software Corp.  All Rights Reserved
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

// sitter
//
#include    <sitter/sitter.h>


// serverplugins
//
#include    <serverplugins/plugin.h>



namespace sitter
{
namespace flags
{



class flags
    : public serverplugins::plugin
{
public:
    SERVERPLUGINS_DEFAULTS(flags);

    // serverplugins::plugin implementation
    virtual void        bootstrap() override;

    // server signal
    void                on_process_watch(as2js::json::json_value_ref & json);

private:
};



} // namespace flags
} // namespace sitter
// vim: ts=4 sw=4 et
