// Snap Websites Server -- Network watchdog
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
namespace network
{



class network
    : public cppthread::plugin
{
public:
    CPPTHREAD_PLUGIN_DEFAULTS(network);

    // cppthread::plugin implementation
    virtual void        bootstrap(void * s) override;

    // server signals
    void                on_process_watch(as2js::JSON & json);

private:
    bool                find_snapcommunicator(as2js::JSON & json);
    bool                verify_snapcommunicator_connection(as2js::JSON & json);

    server *            f_server = nullptr;
    std::string         f_network_data_path = std::string();
};

} // namespace network
} // namespace sitter
// vim: ts=4 sw=4 et
