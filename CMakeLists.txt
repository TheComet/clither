cmake_minimum_required (VERSION 3.24)

if (NOT DEFINED CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif()
message (STATUS "Build type: ${CMAKE_BUILD_TYPE}")

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

project ("clither"
    VERSION 0.0.1
    LANGUAGES C)

###############################################################################
# Config and options
###############################################################################

include (CMakeDependentOption)
option (CLITHER_LOGGING "Enable logging functionality" ON)
cmake_dependent_option (CLITHER_MEMORY_DEBUGGING "Enable tracking malloc/free to find memory bugs" ON "${CMAKE_BUILD_TYPE} STREQUAL Debug" OFF)
set (CLITHER_RENDERER "SDL" CACHE STRING "Set to an empty string or OFF if you only want to build a server (headless) application.")
set_property (CACHE CLITHER_RENDERER PROPERTY STRINGS "OFF;SDL")

set (CLITHER_BUILD_BINDIR "${PROJECT_BINARY_DIR}/bin")
set (CLITHER_BUILD_LIBDIR "${CLITHER_BUILD_BINDIR}/lib")

set (CLITHER_INSTALL_BINDIR ".")
set (CLITHER_INSTALL_LIBDIR "lib")

# Determine platform
if (WIN32 OR CYGWIN)
    set (CLITHER_PLATFORM "WIN32")
elseif (APPLE)
    set (CLITHER_PLATFORM "OSX")
elseif (UNIX)
    set (CLITHER_PLATFORM "LINUX")
else ()
    message (FATAL_ERROR "Platform detection failed")
endif ()

configure_file ("templates/config.h.in" "include/clither/config.h")

###############################################################################
# Executable target
###############################################################################

add_executable (clither
    "src/args.c"
    "src/bezier.c"
    "src/event.c"
    "src/log.c"
    "src/main.c"

    "include/clither/args.h"
    "include/clither/bezier.h"
    "include/clither/event.h"
    "include/clither/log.h"
    "include/clither/net.h"
    "include/clither/render.h"

    "${PROJECT_BINARY_DIR}/include/clither/config.h"

    "templates/config.h.in"

    $<$<STREQUAL:${CLITHER_PLATFORM},WIN32>:src/net_win32.c>
    $<$<STREQUAL:${CLITHER_PLATFORM},LINUX>:src/net_posix.c>
    $<$<STREQUAL:${CLITHER_RENDERER},SDL>:src/render_sdl.c>)
target_compile_options (clither
    PRIVATE
        $<$<C_COMPILER_ID:GNU>:-W -Wall -Wextra -pedantic>
        $<$<C_COMPILER_ID:Clang>:-W -Wall -Wextra -pedantic>)
target_include_directories (clither
    PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>)
set_target_properties (clither
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY ${CLITHER_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY ${CLITHER_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CLITHER_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CLITHER_BUILD_BINDIR}
        INSTALL_RPATH ${CLITHER_INSTALL_LIBDIR})

###############################################################################
# Dependencies
###############################################################################

if (CLITHER_RENDERER STREQUAL "SDL")
    find_package (SDL2 REQUIRED)
    find_package (SDL2_gfx REQUIRED)
    target_link_libraries (clither
        PRIVATE
            SDL2::Core
            SDL2::GFX)
endif ()

###############################################################################
# Install targets
###############################################################################

install (
    TARGETS clither
    RUNTIME DESTINATION ${CLITHER_INSTALL_BINDIR})
