// Copyright (c) 2013-2022  Made to Order Software Corp.  All Rights Reserved.
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

// sitter
//
#include    "sitter/sitter.h"


// cppthread
//
//#include    <cppthread/plugins.h>



namespace sitter
{
namespace apt
{



class apt
    : public cppthread::plugin
{
public:
                        apt();
                        apt(apt const & rhs) = delete;
    virtual             ~apt() override;

    apt &               operator = (apt const & rhs) = delete;

    static apt *        instance();

    // cppthread::plugin implementation
    virtual void        bootstrap(void * server) override;

    // server signal
    void                on_process_watch(as2js::JSON & json);

private:
    server *            f_server = nullptr;
};



} // namespace apt
} // namespace sitter
// vim: ts=4 sw=4 et
