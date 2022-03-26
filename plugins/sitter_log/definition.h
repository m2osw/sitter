// Copyright (c) 2011-2022  Made to Order Software Corp.  All Rights Reserved.
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

// self
//
#include    <search.h>


// advgetopt
//
#include    <advgetopt/utils.h>



namespace sitter
{
namespace log
{



class definition
{
public:
    typedef std::vector<definition>         vector_t;

    static constexpr size_t const           MAX_SIZE_UNDEFINED = 0;

                                definition(std::string const & name, bool mandatory);

    void                        set_mandatory(bool mandatory);
    void                        set_secure(bool secure);
    void                        set_path(std::string const & path);
    void                        set_user_name(std::string const & user_name);
    void                        set_group_name(std::string const & group_name);
    void                        set_mode(int mode);
    void                        set_mode_mask(int mode_mask);
    void                        add_pattern(std::string const & pattern);
    void                        set_max_size(std::size_t size);
    void                        add_search(search const & s);

    std::string const &         get_name() const;
    bool                        is_mandatory() const;
    bool                        is_secure() const;
    std::string const &         get_path() const;
    uid_t                       get_uid() const;
    gid_t                       get_gid() const;
    mode_t                      get_mode() const;
    mode_t                      get_mode_mask() const;
    advgetopt::string_list_t const &
                                get_patterns() const;
    size_t                      get_max_size() const;
    search::vector_t            get_searches() const;


private:
    std::string                 f_name          = std::string();
    std::string                 f_path          = std::string("/var/log/snapwebsites");
    advgetopt::string_list_t    f_patterns      = { std::string("*.log") };
    std::size_t                 f_max_size      = MAX_SIZE_UNDEFINED;
    uid_t                       f_uid           = -1;
    gid_t                       f_gid           = -1;
    int                         f_mode          = 0;
    int                         f_mode_mask     = 07777; // i.e. no masking
    search::vector_t            f_searches      = search::vector_t();
    bool                        f_mandatory     = false;
    bool                        f_secure        = false;
    bool                        f_first_pattern = true;
};


definition::vector_t       load();



} // namespace log
} // namespace sitter
// vim: ts=4 sw=4 et
