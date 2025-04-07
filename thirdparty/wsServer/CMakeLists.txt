# Copyright (C) 2016-2022  Davidson Francis <davidsondfgl@gmail.com>
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
# along with this program.  If not, see <http://www.gnu.org/licenses/>

cmake_minimum_required (VERSION 3.5.2)

project (ws
    LANGUAGES C)

option (WS_TESTS "Enable wsServer testing (requires Autobahn)" OFF)
option (WS_VALIDATE_UTF8 "Enable UTF-8 validation (default ON)" ON)
option (WS_EXAMPLES "Build examples" OFF)
option (WS_EXTRAS "Build extras" OFF)

find_package (Threads REQUIRED)

add_library (ws
    src/ws.c
    src/base64.c
    src/sha1.c
    src/handshake.c
    src/utf8.c)
target_include_directories (ws
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
set_target_properties (ws
    PROPERTIES
        C_STANDARD 99)
target_link_libraries (ws
    PRIVATE
        $<$<PLATFORM_ID:Windows>:ws2_32>
        Threads::Threads)
if (WS_VALIDATE_UTF8)
	target_compile_definitions (ws PRIVATE WS_VALIDATE_UTF8)
endif(WS_VALIDATE_UTF8)

if (WS_EXTRAS)
    add_executable (toyws_test
        extra/toyws/tws_test.c
        extra/toyws/toyws.c)
endif ()

if (WS_EXAMPLES)
    add_executable (toyws_test
        extra/toyws/tws_test.c
        extra/toyws/toyws.c)
    add_subdirectory (examples)
endif ()

if (WS_TESTS)
	enable_testing ()
	# Disable verbose output of echo
	target_compile_definitions (echo PRIVATE DISABLE_VERBOSE)
	add_subdirectory (tests)
endif (WS_TESTS)

