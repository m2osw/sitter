// Snap Websites Server -- watchdog packages
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

// sitter
//
#include    <sitter/sitter.h>


// serverplugins
//
#include    <serverplugins/plugin.h>



namespace sitter
{
namespace packages
{



class packages
    : public serverplugins::plugin
{
public:
    SERVERPLUGINS_DEFAULTS(packages);

    // plugins::plugin implementation
    virtual void        bootstrap() override;

    // server signals
    void                on_process_watch(as2js::JSON::JSONValueRef & json);

private:
    void                load_packages();
    void                load_package(int index, std::string package_filename);
    void                load_json(std::string package_filename);
};



} // namespace packages
} // namespace sitter
// vim: ts=4 sw=4 et
