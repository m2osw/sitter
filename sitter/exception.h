// Copyright (c) 2013-2023  Made to Order Software Corp.  All Rights Reserved.
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
// Snap Websites Server -- snapwebsites flag functionality
#pragma once

// libexcept
//
#include    <libexcept/exception.h>



namespace sitter
{



DECLARE_LOGIC_ERROR(logic_error);

DECLARE_OUT_OF_RANGE(out_of_range);

DECLARE_MAIN_EXCEPTION(sitter_exception);

DECLARE_EXCEPTION(sitter_exception, invalid_name);
DECLARE_EXCEPTION(sitter_exception, invalid_parameter);
DECLARE_EXCEPTION(sitter_exception, invalid_priority);
DECLARE_EXCEPTION(sitter_exception, missing_parameter);
DECLARE_EXCEPTION(sitter_exception, no_server);
DECLARE_EXCEPTION(sitter_exception, too_many_flags);



} // namespace sitter
// vim: ts=4 sw=4 et
