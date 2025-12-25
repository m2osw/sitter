// Copyright (c) 2011-2025  Made to Order Software Corp.  All Rights Reserved.
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


// self
//
#include    "./log.h"



// sitter
//
#include    <sitter/exception.h>


// snaplogger
//
#include    <snaplogger/message.h>


// snapdev
//
#include    <snapdev/chownnm.h>
#include    <snapdev/enumerate.h>
#include    <snapdev/glob_to_list.h>
#include    <snapdev/mounts.h>
#include    <snapdev/not_reached.h>
#include    <snapdev/not_used.h>


// serverplugins
//
#include    <serverplugins/collection.h>


// C
//
#include    <sys/stat.h>


// last include
//
#include    <snapdev/poison.h>



namespace sitter
{
namespace log
{

SERVERPLUGINS_START(log)
    , ::serverplugins::description(
            "Check log files existance, size, ownership, and permissions.")
    , ::serverplugins::dependency("server")
    , ::serverplugins::help_uri("https://snapwebsites.org/help")
    , ::serverplugins::categorization_tag("log")
SERVERPLUGINS_END(log)




/** \brief Initialize log.
 *
 * This function terminates the initialization of the log plugin
 * by registering for different events.
 */
void log::bootstrap()
{
    SERVERPLUGINS_LISTEN(log, server, process_watch, boost::placeholders::_1);
}


/** \brief Process this sitter data.
 *
 * This function runs this plugin actual check.
 *
 * \param[in] json  The document where the results are collected.
 */
void log::on_process_watch(as2js::json::json_value_ref & json)
{
    SNAP_LOG_DEBUG
        << "log::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    definition::vector_t log_defs(load());

    as2js::json::json_value_ref e(json["logs"]);

    // check each log
    //
    size_t const max_logs(log_defs.size());
    for(size_t idx(0); idx < max_logs; ++idx)
    {
        definition const & def(log_defs[idx]);
        std::string const & path(def.get_path());
        advgetopt::string_list_t const & patterns(def.get_patterns());
        f_found = false;
        for(auto const & p : patterns)
        {
            snapdev::glob_to_list<std::list<std::string>> log_filenames;
            log_filenames.read_path<
                  snapdev::glob_to_list_flag_t::GLOB_FLAG_NO_ESCAPE
                , snapdev::glob_to_list_flag_t::GLOB_FLAG_IGNORE_ERRORS>(path + "/" + p);
            snapdev::enumerate(
                  log_filenames
                , std::bind(&log::check_log, this, std::placeholders::_1, std::placeholders::_2, def, e));
        }
        if(!f_found)
        {
            std::string const err_msg(
                      "no logs found for "
                    + def.get_name()
                    + " which says it is mandatory to have at least one log file");
            plugins()->get_server<sitter::server>()->append_error(
                  e
                , "log"
                , err_msg
                , 85); // priority
        }
    }
}


void log::check_log(
      int index
    , std::string filename
    , definition const & def
    , as2js::json::json_value_ref & json)
{
    snapdev::NOT_USED(index);

    struct stat st;
    if(stat(filename.c_str(), &st) == 0)
    {
        // found at least one log under that directory with that pattern
        //
        f_found = true;

        as2js::json::json_value_ref l(json["log"]);

        l["name"] = def.get_name();
        l["filename"] = filename;
        l["size"] = st.st_size;
        l["mode"] = st.st_mode;
        l["uid"] = st.st_uid;
        l["gid"] = st.st_gid;
        l["mtime"] = st.st_mtime; // we could look into showing the timespec instead?

        if(static_cast<size_t>(st.st_size) > def.get_max_size())
        {
            // file is too big, generate an error about it!
            //
            std::string const err_msg(
                      "size of log file  "
                    + def.get_name()
                    + " ("
                    + filename
                    + ") is "
                    + std::to_string(st.st_size)
                    + ", which is more than the maximum size of "
                    + std::to_string(def.get_max_size()));
            plugins()->get_server<sitter::server>()->append_error(
                  l
                , "log"
                , err_msg
                , static_cast<size_t>(st.st_size) > def.get_max_size() * 2 ? 73 : 58); // priority
        }

        uid_t const uid(def.get_uid());
        if(uid != snapdev::NO_UID
        && uid != st.st_uid)
        {
            // file ownership mismatch
            //
            std::string const err_msg(
                      "log file owner mismatched for "
                    + def.get_name()
                    + " ("
                    + filename
                    + "), found "
                    + std::to_string(st.st_uid)
                    + " expected "
                    + std::to_string(uid));
            plugins()->get_server<sitter::server>()->append_error(
                  l
                , "log"
                , err_msg
                , 63); // priority
        }

        gid_t const gid(def.get_gid());
        if(gid != static_cast<gid_t>(-1)
        && gid != st.st_gid)
        {
            // file ownership mismatch
            //
            std::string const err_msg(
                      "log file group mismatched for "
                    + def.get_name()
                    + " ("
                    + filename
                    + "), found "
                    + std::to_string(st.st_gid)
                    + " expected "
                    + std::to_string(gid));
            plugins()->get_server<sitter::server>()->append_error(
                  l
                , "log"
                , err_msg
                , 59); // priority
        }

        mode_t const mode(def.get_mode());
        mode_t const mode_mask(def.get_mode_mask());
        if(mode != 0
        && (st.st_mode & mode_mask) != mode)
        {
            // file ownership mismatch
            //
            std::string const err_msg(
                      "log file mode mismatched "
                    + def.get_name()
                    + " ("
                    + filename
                    + "), found "
                    + std::to_string(st.st_mode)    // TODO: get octal
                    + " expected "
                    + std::to_string(mode));        // TODO: get octal
            plugins()->get_server<sitter::server>()->append_error(
                  l
                , "log"
                , err_msg
                , 64); // priority
        }

        // TODO: do the searches if we have some regex defined
    }
    else
    {
        // file does not exist anymore or we have a permission problem?
    }
}



} // namespace log
} // namespace sitter
// vim: ts=4 sw=4 et
