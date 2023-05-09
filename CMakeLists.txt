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
option (CLITHER_BENCHMARKS "Compile benchmarks (requires C++)" ON)
option (CLITHER_LOGGING "Enable logging functionality" ON)
set (CLITHER_GFX "SDL" CACHE STRING "Set to an empty string or OFF if you only want to build a server (headless) application.")
set_property (CACHE CLITHER_GFX PROPERTY STRINGS "OFF;SDL")
option (CLITHER_TESTS "Compile unit tests (requires C++)" ON)

set (CLITHER_BUILD_BINDIR "${PROJECT_BINARY_DIR}/bin")
set (CLITHER_BUILD_LIBDIR "${CLITHER_BUILD_BINDIR}/lib")

set (CLITHER_INSTALL_BINDIR ".")
set (CLITHER_INSTALL_LIBDIR "lib")

configure_file ("templates/config.h.in" "include/clither/config.h")

###############################################################################
# Game source files
###############################################################################

# We declare these as a library so it can be linked into multiple
# executables further down (tests, benchmarks, and the main exe)
add_library (clither_lib STATIC
    "templates/config.h.in"
    "${PROJECT_BINARY_DIR}/include/clither/config.h"

    "include/clither/args.h"
    "include/clither/bezier.h"
    "include/clither/controls.h"
    "include/clither/event.h"
    "include/clither/gfx.h"
    "include/clither/log.h"
    "include/clither/msg.h"
    "include/clither/net.h"
    "include/clither/protocol.h"
    "include/clither/q.h"
    "include/clither/signals.h"
    "include/clither/snake.h"
    "include/clither/tick.h"
    "include/clither/world.h"

    "src/args.c"
    "src/bezier.c"
    "src/event.c"
    $<$<STREQUAL:${CLITHER_GFX},SDL>:src/gfx_sdl.c>
    "src/log.c"
    "src/msg.c"
    "src/net.c"
    "src/protocol.c"
    $<$<PLATFORM_ID:Windows>:
        src/signals_win32.c
        src/tick_win32.c>
    $<$<PLATFORM_ID:Linux>:
        src/signals_posix.c
        src/tick_posix.c>
    $<$<PLATFORM_ID:Darwin>:
        src/signals_posix.c
        src/tick_posix.c>
    "src/snake.c"
    "src/world.c")
target_include_directories (clither_lib
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_compile_options (clither_lib
    PRIVATE
        $<$<C_COMPILER_ID:GNU>:-W -Wall -Wextra -pedantic>
        $<$<C_COMPILER_ID:Clang>:-W -Wall -Wextra -pedantic>)
set_target_properties (clither_lib
    PROPERTIES
        OUTPUT_NAME "clither")

###############################################################################
# Dependencies
###############################################################################

add_subdirectory ("cstructures")

target_link_libraries (clither_lib
    PUBLIC
        cstructures
    PRIVATE
        $<$<PLATFORM_ID:Windows>:ws2_32>)

if (CLITHER_GFX STREQUAL "SDL")
    find_package (SDL2 REQUIRED)
    #find_package (SDL2_gfx REQUIRED)
    target_link_libraries (clither_lib
        PUBLIC
            SDL2::Core)
            #SDL2::GFX)

    if (WIN32 OR CYGWIN)
        add_custom_command (TARGET clither_lib POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SDL2_PATH}/lib/$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,x64,x86>/SDL2.dll ${CLITHER_BUILD_BINDIR}/SDL2.dll
            COMMENT "Copying SDL2 DLLs to bin directory"
            VERBATIM)
    endif ()
endif ()

###############################################################################
# Main executable
###############################################################################

add_executable (clither
    "src/main.c")
target_compile_options (clither
    PRIVATE
        $<$<C_COMPILER_ID:GNU>:-W -Wall -Wextra -pedantic>
        $<$<C_COMPILER_ID:Clang>:-W -Wall -Wextra -pedantic>)
target_link_libraries (clither
    PRIVATE
        clither_lib)
set_target_properties (clither
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY ${CLITHER_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY ${CLITHER_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CLITHER_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CLITHER_BUILD_BINDIR}
        INSTALL_RPATH ${CLITHER_INSTALL_LIBDIR})

###############################################################################
# Unit tests
###############################################################################

if (CLITHER_TESTS)
    enable_language (CXX)
    add_subdirectory ("thirdparty/googletest")
    add_executable (clither_tests
        "tests/main.cpp"

        "tests/clither/test_args.cpp"
        "tests/clither/test_protocol.cpp"
        "tests/clither/test_tick.cpp"

        "tests/cstructures/env_library_init.cpp"
        "tests/cstructures/test_btree_as_set.cpp"
        "tests/cstructures/test_btree.cpp"
        "tests/cstructures/test_hashmap.cpp"
        "tests/cstructures/test_vector.cpp")
    target_link_libraries (clither_tests
        PRIVATE
            clither_lib
            gmock)
    set_target_properties (clither_tests
        PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY ${CLITHER_BUILD_BINDIR}
            RUNTIME_OUTPUT_DIRECTORY ${CLITHER_BUILD_BINDIR}
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CLITHER_BUILD_BINDIR}
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CLITHER_BUILD_BINDIR}
            INSTALL_RPATH ${CLITHER_INSTALL_LIBDIR})
endif ()

###############################################################################
# Benchmarks
###############################################################################

if (CLITHER_BENCHMARKS)
    enable_language (CXX)
    add_subdirectory ("thirdparty/benchmark")
    add_executable (clither_benchmarks
        "benchmarks/main.cpp"

        "benchmarks/cstructures/bench_hashmap.cpp"
        "benchmarks/cstructures/bench_std_unordered_map.cpp"
        "benchmarks/cstructures/bench_std_vector.cpp"
        "benchmarks/cstructures/bench_vector.cpp")
    target_link_libraries (clither_benchmarks
        PRIVATE
            clither_lib
            benchmark)
    set_target_properties (clither_benchmarks
        PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY ${CLITHER_BUILD_BINDIR}
            RUNTIME_OUTPUT_DIRECTORY ${CLITHER_BUILD_BINDIR}
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CLITHER_BUILD_BINDIR}
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CLITHER_BUILD_BINDIR}
            INSTALL_RPATH ${CLITHER_INSTALL_LIBDIR})
endif ()

###############################################################################
# Install targets
###############################################################################

install (
    TARGETS clither
    RUNTIME DESTINATION ${CLITHER_INSTALL_BINDIR})
