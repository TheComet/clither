# ****************************************************************************
#  Project:  LibCMaker_HarfBuzz
#  Purpose:  A CMake build script for HarfBuzz Library
#  Author:   NikitaFeodonit, nfeodonit@yandex.com
# ****************************************************************************
#    Copyright (c) 2017-2018 NikitaFeodonit
#
#    This file is part of the LibCMaker_HarfBuzz project.
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published
#    by the Free Software Foundation, either version 3 of the License,
#    or (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#    See the GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program. If not, see <http://www.gnu.org/licenses/>.
# ****************************************************************************

#.rst:
# Find HarfBuzz
# -------------
#
# Finds the HarfBuzz library. This module defines:
#
#  HarfBuzz_FOUND           - True if HarfBuzz library is found
#  HarfBuzz::HarfBuzz       - HarfBuzz imported target
#
# Additionally these variables are defined for internal usage:
#
#  HARFBUZZ_LIBRARY         - HarfBuzz library
#  HARFBUZZ_LIBRARIES       - Same as HARFBUZZ_LIBRARY
#  HARFBUZZ_INCLUDE_DIR     - Include dir
#

#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
#             Vladimír Vondruš <mosra@centrum.cz>
#
#   Permission is hereby granted, free of charge, to any person obtaining a
#   copy of this software and associated documentation files (the "Software"),
#   to deal in the Software without restriction, including without limitation
#   the rights to use, copy, modify, merge, publish, distribute, sublicense,
#   and/or sell copies of the Software, and to permit persons to whom the
#   Software is furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be included
#   in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#   DEALINGS IN THE SOFTWARE.
#

# Include dir
find_path(HARFBUZZ_INCLUDE_DIR
  NAMES
    hb.h
  PATH_SUFFIXES
    include/harfbuzz
    include
    harfbuzz)

# Library
find_library(HARFBUZZ_LIBRARY
  NAMES
    harfbuzzd harfbuzz
  PATH_SUFFIXES
    lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HarfBuzz DEFAULT_MSG
  HARFBUZZ_LIBRARY
  HARFBUZZ_INCLUDE_DIR
)

mark_as_advanced(FORCE
  HARFBUZZ_LIBRARY
  HARFBUZZ_INCLUDE_DIR
)

if(HarfBuzz_FOUND AND NOT TARGET HarfBuzz::HarfBuzz)
  add_library(HarfBuzz::HarfBuzz UNKNOWN IMPORTED)
  set_target_properties(HarfBuzz::HarfBuzz PROPERTIES
    IMPORTED_LOCATION ${HARFBUZZ_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${HARFBUZZ_INCLUDE_DIR})
endif()

set(HARFBUZZ_LIBRARIES ${HARFBUZZ_LIBRARY})
set(HARFBUZZ_INCLUDE_DIRS ${HARFBUZZ_INCLUDE_DIR})
