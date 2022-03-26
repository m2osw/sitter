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
// Snap Websites Server -- Flags watchdog: check for raised flags


// self
//
#include    "flags.h"


// sitter
//
#include    <sitter/flags.h>


// snaplogger
//
#include    <snaplogger/message.h>


// snapdev
//
#include    <snapdev/not_reached.h>
#include    <snapdev/not_used.h>


// last include
//
#include    <snapdev/poison.h>



namespace sitter
{
namespace flags
{


CPPTHREAD_PLUGIN_START(flags, 1, 0)
    , ::cppthread::plugin_description(
            "Check raised flags and generate errors accordingly.")
    , ::cppthread::plugin_dependency("server")
    , ::cppthread::plugin_help_uri("https://snapwebsites.org/help")
    , ::cppthread::plugin_categorization_tag("flag")
CPPTHREAD_PLUGIN_END(flags)




/** \brief Initialize flags.
 *
 * This function terminates the initialization of the flags plugin
 * by registering for different events.
 *
 * \param[in] s  The sitter server.
 */
void flags::bootstrap(void * s)
{
    f_server = static_cast<server *>(s);

    SNAP_LISTEN(flags, "server", server, process_watch, boost::placeholders::_1);
}


/** \brief Process this watchdog data.
 *
 * This function runs this watchdog.
 *
 * \param[in] doc  The document.
 */
void flags::on_process_watch(as2js::JSON::JSONValueRef & json)
{
    SNAP_LOG_DEBUG
        << "flags::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    // check whether we have any flags that are currently raised
    // if not, we just return ASAP
    //
    sitter::flag::vector_t list(sitter::flag::load_flags());
    if(list.empty())
    {
        return;
    }

    as2js::JSON::JSONValueRef & flg(json["flags"]);

    // add each flag to the DOM
    //
    int max_priority(5);
    std::string names;
    for(auto f : list)
    {
        as2js::JSON::JSONValueRef & flag(flg[-1]);

        std::string const name(f->get_name());
        int const priority(f->get_priority());

        flag["unit"] =        f->get_unit();
        flag["section"] =     f->get_section();
        flag["name"] =        name;
        flag["priority"] =    priority;
        flag["manual-down"] = f->get_manual_down();
        flag["date"] =        f->get_date(); // time_t
        flag["modified"] =    f->get_modified(); // time_t
        flag["message"] =     f->get_message();
        flag["source-file"] = f->get_source_file();
        flag["function"] =    f->get_function();
        flag["line"] =        f->get_line();

        snap_flag::tag_list_t const & tag_list(f->get_tags());
        if(!tag_list.empty())
        {
            as2js::JSON::JSONValueRef & tags(flag["tags"]);
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

    f_snap->append_error(
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
