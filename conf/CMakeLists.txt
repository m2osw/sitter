# Copyright (c) 2011-2022  Made to Order Software Corp.  All Rights Reserved
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

# Configuration files
install(
    FILES
        sitter.conf

    DESTINATION
        share/sitter
)

# Log definitions (i.e. how to verify that log files are in good standing)
install(
    DIRECTORY
        log-definitions

    DESTINATION
        share/sitter
)

# Logger files
install(
    FILES
        logger/sitter.conf

    DESTINATION
        ${SNAPLOGGER_SHARE_CONF_DIR}
)

# test MTA file
install(
    FILES
        test-mta.txt

    DESTINATION
        lib/sitter
)

# sitter definitions for fluid-settings
install(
    FILES
        sitter.ini

    DESTINATION
        ${FLUIDSETTINGS_DEFINITIONS_INSTALL_DIR}
)

# Define as a Communicator Service
install(
    FILES
        sitter.service

    DESTINATION
        ${COMMUNICATORD_SERVICES_INSTALL_DIR}
)


# vim: ts=4 sw=4 et
