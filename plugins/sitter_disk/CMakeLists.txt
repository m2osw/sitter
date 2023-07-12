# Copyright (c) 2011-2023  Made to Order Software Corp.  All Rights Reserved
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

AtomicNames("names.an")

project(sitter_disk)

add_library(${PROJECT_NAME} SHARED
    disk.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/names.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${SNAPDEV_INCLUDE_DIRS}
)

install(
    TARGETS
        ${PROJECT_NAME}

    LIBRARY DESTINATION
        ${PLUGIN_INSTALL_DIR}
)

install(
    DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}

    DESTINATION
        include/sitter/plugins/disk

    FILES_MATCHING PATTERN
        "*.h"
)

# definitions for fluid-settings
install(
    FILES
        sitter-disk.ini

    DESTINATION
        ${FLUIDSETTINGS_DEFINITIONS_INSTALL_DIR}
)

# vim: ts=4 sw=4 et
