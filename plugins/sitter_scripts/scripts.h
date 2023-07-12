// Copyright (c) 2013-2023  Made to Order Software Corp.  All Rights Reserved
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
namespace scripts
{



class scripts
    : public serverplugins::plugin
{
public:
    SERVERPLUGINS_DEFAULTS(scripts);

    // serverplugins::plugin implementation
    virtual void            bootstrap() override;

    // server signals
    void                    on_process_watch(as2js::json::json_value_ref & json);

private:
    void                    process_script(int index, std::string script_filename);
    static std::string      format_date(time_t const t);
    std::string             generate_header(std::string const & type);

    as2js::json::json_value_ref
                            f_scripts = as2js::json::json_value_ref(
                                          std::make_shared<as2js::json::json_value>(as2js::position(), as2js::json::json_value::object_t())
                                        , std::string("undefined"));

    std::string             f_script_starter = std::string();
    std::string             f_log_path = std::string();
    std::string             f_log_subfolder = std::string();

    std::string             f_scripts_output_log = std::string();
    std::string             f_scripts_error_log = std::string();

    std::string             f_script_filename = std::string();
    time_t                  f_start_date = time_t();
    std::string             f_email = std::string();
};


} // namespace scripts
} // namespace sitter
// vim: ts=4 sw=4 et
