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


// self
//
#include    "search.h"


//// advgetopt
////
//#include    <advgetopt/conf_file.h>
//#include    <advgetopt/validator_size.h>
//
//
//// snaplogger
////
//#include    <snaplogger/message.h>
//
//
//// snapdev
////
//#include    <snapdev/enumerate.h>
//#include    <snapdev/glob_to_list.h>
//#include    <snapdev/not_used.h>
//#include    <snapdev/trim_string.h>
//
//
//// C++
////
////#include    <functional>
//
//
//// C
////
//#include    <grp.h>
//#include    <pwd.h>


// last include
//
#include    <snapdev/poison.h>




namespace sitter
{
namespace log
{



search::search(std::string const & regex, std::string const & report_as)
    : f_regex(regex)
    , f_report_as(report_as)
{
}


std::string const & search::get_regex() const
{
    return f_regex;
}


std::string const & search::get_report_as() const
{
    return f_report_as;
}



} // namespace log
} // namespace sitter
// vim: ts=4 sw=4 et
