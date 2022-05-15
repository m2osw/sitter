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


// self
//
#include    "flags.h"


// communicatord
//
#include    <communicatord/flags.h>


// snaplogger
//
#include    <snaplogger/message.h>


// snapdev
//
#include    <snapdev/not_reached.h>
#include    <snapdev/not_used.h>


// serverplugins
//
#include    <serverplugins/collection.h>


// last include
//
#include    <snapdev/poison.h>



namespace sitter
{
namespace flags
{


SERVERPLUGINS_START(flags, 1, 0)
    , ::serverplugins::description(
            "Check raised flags and generate errors accordingly.")
    , ::serverplugins::dependency("server")
    , ::serverplugins::help_uri("https://snapwebsites.org/help")
    , ::serverplugins::categorization_tag("flag")
SERVERPLUGINS_END(flags)




/** \brief Initialize flags.
 *
 * This function terminates the initialization of the flags plugin
 * by registering for different events.
 */
void flags::bootstrap()
{
    SERVERPLUGINS_LISTEN(flags, "server", server, process_watch, boost::placeholders::_1);
}


/** \brief Process this sitter data.
 *
 * This function runs this plugin actual check.
 *
 * \param[in] json  The document where the results are collected.
 */
void flags::on_process_watch(as2js::JSON::JSONValueRef & json)
{
    SNAP_LOG_DEBUG
        << "flags::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    // check whether we have any flags that are currently raised
    // if not, we just return ASAP
    //
    communicatord::flag::list_t list(sitter::flag::load_flags());
    if(list.empty())
    {
        return;
    }

    as2js::JSON::JSONValueRef flg(json["flags"]);

    // add each flag to the DOM
    //
    int max_priority(5);
    std::string names;
    for(auto f : list)
    {
        as2js::JSON::JSONValueRef e(flg[-1]);

        std::string const name(f->get_name());
        int const priority(f->get_priority());

        e["unit"] =        f->get_unit();
        e["section"] =     f->get_section();
        e["name"] =        name;
        e["priority"] =    priority;
        e["manual-down"] = f->get_manual_down();
        e["date"] =        f->get_date(); // time_t
        e["modified"] =    f->get_modified(); // time_t
        e["message"] =     f->get_message();
        e["source-file"] = f->get_source_file();
        e["function"] =    f->get_function();
        e["line"] =        f->get_line();

        sitter::flag::tag_list_t const & tag_list(f->get_tags());
        if(!tag_list.empty())
        {
            as2js::JSON::JSONValueRef tags(flg["tags"]);
            for(auto const & t : tag_list)
            {
                tags[-1] = t;
            }
        }

        if(!names.empty())
        {
            names += ", ";
        }
        names += name;

        max_priority = std::max(max_priority, priority);
    }

    plugins()->get_server<sitter::server>()->append_error(
              flg
            , "flags"
            , std::to_string(list.size())
                + " flag"
                + (list.size() == 1 ? "" : "s")
                + (list.size() == 1 ? "is" : "are")
                + " raised -- "
                + names
            , max_priority);
}



} // namespace flags
} // namespace sitter
// vim: ts=4 sw=4 et
