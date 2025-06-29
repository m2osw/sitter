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
#pragma once

// C++
//
#include    <string>
#include    <vector>



namespace sitter
{
namespace log
{



class search
{
public:
    typedef std::vector<search> vector_t;

                                search(
                                      std::string const & regex
                                    , std::string const & report_as);

    std::string const &         get_regex() const;
    std::string const &         get_report_as() const;

private:
    std::string                 f_regex = std::string();
    std::string                 f_report_as = std::string("error");
};



} // namespace log
} // namespace sitter
// vim: ts=4 sw=4 et
