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

// cppthreadd
//
#include    <cppthread/runner.h>


// as2js
//
#include    <as2js/json.h>


// serverplugins
//
#include    <serverplugins/collection.h>



namespace sitter
{


class server;

class sitter_worker
    : public cppthread::runner
{
public:
    typedef std::shared_ptr<sitter_worker>
                            pointer_t;

                            sitter_worker(std::shared_ptr<server> s);
                            sitter_worker(sitter_worker const &) = delete;
    virtual                 ~sitter_worker();
    sitter_worker &         operator = (sitter_worker const &) = delete;

    // cppthread::runner implementation
    //
    virtual void            run();

    void                    tick();

private:
    void                    load_plugins();
    void                    loop();
    void                    wait_next_tick();
    void                    run_plugins();
    void                    report_error(as2js::JSON & json, time_t start_date);

    std::shared_ptr<server> f_server = std::shared_ptr<server>();
    int                     f_ticks = 0;
    cppthread::mutex        f_mutex = cppthread::mutex();
    serverplugins::collection::pointer_t
                            f_plugins = serverplugins::collection::pointer_t();
};


} // namespace sitter
// vim: ts=4 sw=4 et
