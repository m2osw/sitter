// Copyright (c) 2013-2025  Made to Order Software Corp.  All Rights Reserved
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
#include    <sitter/sitter.h>


// serverplugins
//
#include    <serverplugins/plugin.h>



namespace sitter
{
namespace certificate
{



SERVERPLUGINS_VERSION(certificate, 1, 0)


class certificate
    : public serverplugins::plugin
{
public:
    SERVERPLUGINS_DEFAULTS(certificate);

    // serverplugins::plugin implementation
    virtual void        bootstrap() override;

    // server signal
    void                on_process_watch(as2js::json::json_value_ref & json);

private:
    void                parse_delays();

    std::map<std::int64_t, std::int64_t>    f_delays_n_priorities = std::map<std::int64_t, std::int64_t>();
    std::map<std::string, time_t>           f_access_error = std::map<std::string, time_t>();
};


} // namespace certificate
} // namespace sitter
// vim: ts=4 sw=4 et
