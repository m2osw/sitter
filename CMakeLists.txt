# Copyright (c) 2011-2025  Made to Order Software Corp.  All Rights Reserved
#
# https://snapwebsites.org/project/sitter
# contact@m2osw.com
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.10.2)

project(sitter_project)

find_package(AdvGetOpt        REQUIRED)
find_package(As2Js            REQUIRED)
find_package(AtomicNames      REQUIRED)
find_package(Boost            REQUIRED)
find_package(Communicator     REQUIRED)
find_package(CppProcess       REQUIRED)
find_package(CppThread        REQUIRED)
find_package(EdHttp           REQUIRED)
find_package(EventDispatcher  REQUIRED)
find_package(FluidSettings    REQUIRED)
find_package(LibAddr          REQUIRED)
find_package(LibExcept        REQUIRED)
find_package(LibMimeMail      REQUIRED)
find_package(LibUtf8          REQUIRED)
find_package(OpenSSL          REQUIRED)
find_package(ServerPlugins    REQUIRED)
find_package(SnapCMakeModules REQUIRED)
find_package(SnapDev          REQUIRED)
find_package(SnapLogger       REQUIRED)

SnapGetVersion(SITTER ${CMAKE_CURRENT_SOURCE_DIR})

enable_language(CXX)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}
)

##
## Compiling
##
add_subdirectory(conf)              # Configuration files and samples
add_subdirectory(sitter)            # Common code which is too specialized to create a separate library
add_subdirectory(daemon)            # Deamon running sitting scripts & plugins
add_subdirectory(plugins)           # Plug-ins (i.e. modular functionality)
add_subdirectory(tools)             # Various tools
add_subdirectory(cmake)             # CMake Config
add_subdirectory(doc)               # Documentation
add_subdirectory(tests)             # Unit tests

# vim: ts=4 sw=4 et nocindent
