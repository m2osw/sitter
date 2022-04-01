// Snap Websites Server -- watchdog scripts
// Copyright (c) 2018-2021  Made to Order Software Corp.  All Rights Reserved
//
// https://snapwebsites.org/
// contact@m2osw.com
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
namespace scripts
{


//enum class name_t
//{
//    SNAP_NAME_WATCHDOG_WATCHSCRIPTS_DEFAULT_LOG_SUBFOLDER,
//    SNAP_NAME_WATCHDOG_WATCHSCRIPTS_LOG_SUBFOLDER,
//    SNAP_NAME_WATCHDOG_WATCHSCRIPTS_OUTPUT,
//    SNAP_NAME_WATCHDOG_WATCHSCRIPTS_OUTPUT_DEFAULT,
//    SNAP_NAME_WATCHDOG_WATCHSCRIPTS_PATH,
//    SNAP_NAME_WATCHDOG_WATCHSCRIPTS_PATH_DEFAULT,
//    SNAP_NAME_WATCHDOG_WATCHSCRIPTS_WATCH_SCRIPT_STARTER,
//    SNAP_NAME_WATCHDOG_WATCHSCRIPTS_WATCH_SCRIPT_STARTER_DEFAULT
//};
//char const * get_name(name_t name) __attribute__ ((const));








class scripts
    : public serverplugins::plugin
{
public:
    SERVERPLUGINS_DEFAULTS(scripts);

    // serverplugins::plugin implementation
    virtual void            bootstrap() override;

    // server signals
    void                    on_process_watch(as2js::JSON::JSONValueRef & json);

private:
    void                    process_script(int index, std::string script_filename);
    static std::string      format_date(time_t const t);
    std::string             generate_header(std::string const & type);

    as2js::JSON::JSONValueRef
                            f_scripts = as2js::JSON::JSONValueRef(
                                          as2js::JSON::JSONValue::pointer_t()
                                        , as2js::String());

    std::string             f_watch_script_starter = std::string();
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
