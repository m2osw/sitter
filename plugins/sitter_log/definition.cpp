// Copyright (c) 2011-2023  Made to Order Software Corp.  All Rights Reserved.
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
#include    "definition.h"


// sitter
//
#include    <sitter/exception.h>


// advgetopt
//
#include    <advgetopt/conf_file.h>
#include    <advgetopt/validator_size.h>


// snaplogger
//
#include    <snaplogger/message.h>


// snapdev
//
#include    <snapdev/enumerate.h>
#include    <snapdev/glob_to_list.h>
#include    <snapdev/not_used.h>
#include    <snapdev/trim_string.h>


// serverplugins
//
#include    <serverplugins/collection.h>


// C
//
#include    <grp.h>
#include    <pwd.h>


// last include
//
#include    <snapdev/poison.h>




namespace sitter
{
namespace log
{



namespace
{





/** \brief Load a log definition XML file.
 *
 * This function loads one log definition XML file and transform it in a
 * definition structure.
 *
 * Note that one file may include many log definitions.
 *
 * \param[in] index  Index of the input string in the list of XML files.
 * \param[in] log_definitions_filename  The name of a configuration file
 *                                      representing a log definition.
 * \param[in,out] result  The vector of log definitions grown on each call.
 */
void load_config(
      std::string const & log_definitions_filename
    , definition::vector_t & result)
{
    advgetopt::conf_file_setup setup(log_definitions_filename);
    advgetopt::conf_file::pointer_t defs(advgetopt::conf_file::get_conf_file(setup));

    if(!defs->has_parameter("name"))
    {
        throw missing_parameter(
                  "the \"name\" parameter is mandatory in a log definition.");
    }
    std::string const name(defs->get_parameter("name"));
    if(name.empty())
    {
        throw invalid_parameter(
                  "the \"name=...\" of a log definition cannot be the empty string.");
    }

    auto it(std::find_if(
                  result.begin()
                , result.end()
                , [name](auto const & l)
                {
                    return name == l.get_name();
                }));
    if(it != result.end())
    {
        throw invalid_parameter(
                  "found log definition named \""
                + name
                + "\" twice.");
    }

    bool mandatory(false);
    if(defs->has_parameter("mandatory"))
    {
        mandatory = advgetopt::is_true(defs->get_parameter("mandatory"));
    }

    definition wl(name, mandatory);

    if(defs->has_parameter("secure"))
    {
        wl.set_secure(advgetopt::is_true(defs->get_parameter("secure")));
    }

    if(defs->has_parameter("path"))
    {
        wl.set_path(defs->get_parameter("path"));
    }

    if(defs->has_parameter("path"))
    {
        std::string const patterns_str(defs->get_parameter("path"));
        advgetopt::string_list_t patterns;
        advgetopt::split_string(patterns_str, patterns, { ":" });
        for(auto p : patterns)
        {
            wl.add_pattern(p);
        }
    }

    if(defs->has_parameter("user_name"))
    {
        wl.set_user_name(defs->get_parameter("user_name"));
    }

    if(defs->has_parameter("group_name"))
    {
        wl.set_group_name(defs->get_parameter("group_name"));
    }

    if(defs->has_parameter("max_size"))
    {
        std::string max_size_str(defs->get_parameter("max_size"));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        __int128 max_size(0);
        if(!advgetopt::validator_size::convert_string(
                      max_size_str
                    , advgetopt::validator_size::VALIDATOR_SIZE_POWER_OF_TWO
                    , max_size))
        {
            throw invalid_parameter(
                      "the \"max-size="
                    + max_size_str
                    + "\" found in log definition \""
                    + log_definitions_filename
                    + "\" is not considered a valid size.");
        }
#pragma GCC diagnostic pop

        wl.set_max_size(static_cast<std::size_t>(max_size));
    }

    if(defs->has_parameter("mode"))
    {
        std::string const mode_str(snapdev::trim_string(
                  defs->get_parameter("mode")
                , true
                , true
                , true));

        int mode(0);
        int mode_mask(0);
        if(!mode_str.empty())
        {
            std::size_t j(0);
            std::size_t len(mode_str.length());

            char c(mode_str[j]);
            if(c >= '0' && c <= '8')
            {
                // octal is read as is
                //
                do
                {
                    mode = mode * 8 + c - '0';
                    ++j;
                    if(j >= len)
                    {
                        break;
                    }
                    c = mode_str[j];
                }
                while(c >= '0' && c <= '8');
                if(j < len
                && c == '/')
                {
                    for(++j; j < len; ++j)
                    {
                        c = mode_str[j];
                        if(c < '0' || c > '8')
                        {
                            throw invalid_parameter(
                                      "invalid numeric mode \""
                                    + mode_str
                                    + "\" found in \""
                                    + log_definitions_filename
                                    + "\"; it must have a numeric mask.");
                        }
                        mode_mask = mode_mask * 8 + c - '0';
                    }
                }
                if(j != len)
                {
                    throw invalid_parameter(
                              "invalid numeric mode \""
                            + mode_str
                            + "\" found in \""
                            + log_definitions_filename
                            + "\"; it must be one or two octal numbers separated by a slash.");
                }
            }
            else
            {
                // accept letters instead:
                //      u -- owner (user)
                //      g -- group
                //      o -- other
                //      a -- all (user, group, other)
                //
                // the a +-= operator to add remove or
                // set to exactly that value
                //
                // then the permissions are:
                //      r -- read
                //      w -- write
                //      x -- execute (access directory)
                //      s -- set user/group ID
                //      t -- sticky bit
                //
                int flags(0);
                char op(0);
                for(;;)
                {
                    switch(c)
                    {
                    case 'u':
                        flags |= 0700;
                        break;

                    case 'g':
                        flags |= 0070;
                        break;

                    case 'o':
                        flags |= 0007;
                        break;

                    case 'a':
                        flags |= 0777;
                        break;

                    case '+':
                    case '-':
                    case '=':
                        if(op != 0)
                        {
                            throw invalid_parameter(
                                      std::string("only one operator can be used in a mode; two ('")
                                    + op
                                    + "' and '"
                                    + c
                                    + "') found in \""
                                    + log_definitions_filename
                                    + "\".");
                        }

                        // default is 'a' if undefined
                        //
                        if(flags == 0)
                        {
                            flags = 0777;
                        }

                        // this is our operator
                        //
                        op = c;
                        break;

                    default:
                        throw invalid_parameter(
                                  std::string("unknown character '")
                                + c
                                + "' for mode and/or operator; expected one or more of u, g, o, a, +, -, or =.");

                    }
                    ++j;
                    if(j >= len
                    || op != 0)
                    {
                        break;
                    }
                    c = mode_str[j];
                }

                // the r/w/... flags now
                //
                int upper_mode(0);
                for(; j < len; ++j)
                {
                    c = mode_str[j];
                    switch(c)
                    {
                    case 'r':
                        mode |= 0004;
                        break;

                    case 'w':
                        mode |= 0002;
                        break;

                    case 'x':
                        mode |= 0001;
                        break;

                    case 's':
                        upper_mode |= 06000;
                        break;

                    case 't':
                        upper_mode |= 01000;
                        break;

                    default:
                        throw invalid_parameter(
                                  std::string("unknown character '")
                                + c
                                + "' for actual mode; expected one or more of r, w, x, s, or t.");

                    }
                }

                // repeat the mode as defined in the left
                // hand side
                //
                mode *= flags;

                // add the upper mode as required
                //
                if((upper_mode & 01000) != 0)
                {
                    mode |= 01000; // 't'
                }
                if((upper_mode & 06000) != 0)
                {
                    mode |= ((flags & 0700) != 0 ? 04000 : 0)   // user 's'
                          | ((flags & 0070) != 0 ? 02000 : 0);  // group 's'
                }

                // now the operator defines the mode versus mask
                //
                switch(op)
                {
                case '+':
                    // '+' means that the specified flags must
                    // be set, but others may be set or not
                    //
                    mode_mask = mode;
                    break;

                case '-':
                    // '-' means that the specified flags must
                    // not be set, but others may be set or not
                    //
                    mode_mask = mode;
                    mode ^= 0777; // we can't set mode to zero--but this is bogus if the user expects all the flags to be zero (which should not be something sought)
                    break;

                case '=':
                    //'=' means that the specified flags must
                    // be exactly as specified
                    //
                    mode_mask = 07777;
                    break;

                default:
                    throw logic_error("unexpected operator, did we add one above and are not properly handling it here?");

                }
            }
        }

        wl.set_mode(mode);
        wl.set_mode_mask(mode_mask == 0 ? 07777 : mode_mask);
    }

    // the patterns to search inside the log files must be defined in a
    // section; one section per pattern definition
    //
    advgetopt::conf_file::sections_t const sections(defs->get_sections());
    for(auto sec : sections)
    {
        std::string field_name(sec + "::regex");
        if(defs->has_parameter(field_name))
        {
            std::string const regex(defs->get_parameter(field_name));
            if(regex.empty())
            {
                throw invalid_parameter(
                          "regular expression cannot be empty in \""
                        + log_definitions_filename
                        + "\".");
            }

            std::string report_as("error");
            field_name = sec + "::report_as";
            if(defs->has_parameter(field_name))
            {
                report_as = defs->get_parameter(field_name);
            }

            search const s(regex, report_as);
            wl.add_search(s);
        }
    }

    result.push_back(wl);
}



}
// no name namespace









/** \class definition
 * \brief Class used to record the list of logs to check.
 *
 * Objects of type definition are read from configuration files.
 *
 * The sitter log plugin checks log files for sizes and various content
 * to warn the administrators of problems it discovers. In most cases,
 * our tools are much more pro-active. They either raise a flag or
 * send a message over the network so that we have no need to check
 * logs. However, third party tools may not offer such capabilities.
 */



definition::definition(std::string const & name, bool mandatory)
    : f_name(name)
    , f_mandatory(mandatory)
{
}


void definition::set_mandatory(bool mandatory)
{
    f_mandatory = mandatory;
}


void definition::set_secure(bool secure)
{
    f_secure = secure;
}


void definition::set_path(std::string const & path)
{
    f_path = path;
}


void definition::set_user_name(std::string const & user_name)
{
    f_uid = -1;

    if(!user_name.empty())
    {
        struct passwd const * pwd(getpwnam(user_name.c_str()));
        if(pwd == nullptr)
        {
            SNAP_LOG_WARNING
                << "user name \""
                << user_name
                << "\" does not exist on this system. A log file can't be owned by that user."
                << SNAP_LOG_SEND;
        }
        else
        {
            f_uid = pwd->pw_uid;
        }
    }
}


void definition::set_group_name(std::string const & group_name)
{
    f_gid = -1;

    if(!group_name.empty())
    {
        struct group const * grp(getgrnam(group_name.c_str()));
        if(grp == nullptr)
        {
            SNAP_LOG_WARNING
                << "group name \""
                << group_name
                << "\" does not exist on this system. A log file can't be owner by that group."
                << SNAP_LOG_SEND;
        }
        else
        {
            f_gid = grp->gr_gid;
        }
    }
}


void definition::set_mode(int mode)
{
    f_mode = mode;
}


void definition::set_mode_mask(int mode_mask)
{
    f_mode_mask = mode_mask;
}


void definition::add_pattern(std::string const & pattern)
{
    if(f_first_pattern)
    {
        f_first_pattern = false;

        f_patterns.clear();
    }
    f_patterns.push_back(pattern);
}


void definition::set_max_size(std::size_t size)
{
    f_max_size = size;
}


void definition::add_search(search const & s)
{
    f_searches.push_back(s);
}


std::string const & definition::get_name() const
{
    return f_name;
}


bool definition::is_mandatory() const
{
    return f_mandatory;
}


bool definition::is_secure() const
{
    return f_secure;
}


std::string const & definition::get_path() const
{
    return f_path;
}


uid_t definition::get_uid() const
{
    return f_uid;
}


gid_t definition::get_gid() const
{
    return f_gid;
}


mode_t definition::get_mode() const
{
    return f_mode;
}


mode_t definition::get_mode_mask() const
{
    return f_mode_mask;
}


advgetopt::string_list_t const & definition::get_patterns() const
{
    return f_patterns;
}


size_t definition::get_max_size() const
{
    return f_max_size;
}


search::vector_t definition::get_searches() const
{
    return f_searches;
}








/** \brief Load the list of sitter log definitions.
 *
 * This function loads the configuration files from the sitter and other
 * packages.
 *
 * \return A vector of definitions objects each representing a log
 *         definition.
 */
definition::vector_t load()
{
    definition::vector_t result;

    snapdev::glob_to_list<std::list<std::string>> log_filenames;
    log_filenames.read_path<
          snapdev::glob_to_list_flag_t::GLOB_FLAG_NO_ESCAPE
        , snapdev::glob_to_list_flag_t::GLOB_FLAG_EMPTY>("/usr/share/sitter/log-definitions/*.conf");
    for(auto log_definitions_filename : log_filenames)
    {
        load_config(log_definitions_filename, result);
    }

    return result;
}



} // namespace log
} // namespace sitter
// vim: ts=4 sw=4 et
