// Copyright (c) 2013-2025  Made to Order Software Corp.  All Rights Reserved
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

/** \file
 * \brief The version of the sitter at run time.
 *
 * This file records the sitter library version at run time.
 *
 * The macros give you the library version at the time you are compiling.
 * The functions allow you to retrieve the version of a dynamically linked
 * library. This implementation defines the functions you can call to
 * retrieve the version at run time.
 */

// self
//
#include    "sitter/version.h"


// last include
//
#include    <snapdev/poison.h>



namespace sitter
{




/** \brief Get the major version of the library
 *
 * This function returns the major version of the running library (the
 * one you are linked against at runtime).
 *
 * \return The major version.
 */
int get_major_version()
{
    return SITTER_VERSION_MAJOR;
}


/** \brief Get the minor version of the library.
 *
 * This function returns the minor version of the running library
 * (the one you are linked against at runtime).
 *
 * \return The release version.
 */
int get_release_version()
{
    return SITTER_VERSION_MINOR;
}


/** \brief Get the patch version of the library.
 *
 * This function returns the patch version of the running library
 * (the one you are linked against at runtime).
 *
 * \return The patch version.
 */
int get_patch_version()
{
    return SITTER_VERSION_PATCH;
}


/** \brief Get the full version of the library as a string.
 *
 * This function returns the major, minor, and patch versions of the
 * running library (the one you are linked against at runtime) in the
 * form of a string.
 *
 * The build version is not made available. In most cases we change
 * the build version only to run a new build, so not code will have
 * changed (some documentation and non-code files may changed between
 * build versions; but the code will work exactly the same way.)
 *
 * \return The library version.
 */
char const * get_version_string()
{
    return SITTER_VERSION_STRING;
}


} // namespace sitter
// vim: ts=4 sw=4 et
