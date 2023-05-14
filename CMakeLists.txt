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

include (CheckCSourceCompiles)
check_c_source_compiles ("__declspec(thread) int value; int main(void) { return 0; }" MSVC_THREADLOCAL)
check_c_source_compiles ("__thread int value; int main(void) { return 0; }" GCC_THREADLOCAL)
if (MSVC_THREADLOCAL)
    set (CLITHER_THREADLOCAL "__declspec(thread)")
endif ()
if (GCC_THREADLOCAL)
    set (CLITHER_THREADLOCAL "__thread")
endif ()

configure_file ("templates/config.h.in" "include/clither/config.h")

###############################################################################
# Game source files
###############################################################################

add_executable (clither
    "templates/config.h.in"
    "${PROJECT_BINARY_DIR}/include/clither/config.h"

    "include/clither/args.h"
    "include/clither/benchmarks.h"
    "include/clither/bezier.h"
    "include/clither/camera.h"
    "include/clither/client.h"
    "include/clither/controls.h"
    "include/clither/event.h"
    "include/clither/gfx.h"
    "include/clither/log.h"
    "include/clither/msg.h"
    "include/clither/msg_queue.h"
    "include/clither/mutex.h"
    "include/clither/net.h"
    "include/clither/q.h"
    "include/clither/server.h"
    "include/clither/server_settings.h"
    "include/clither/signals.h"
    "include/clither/snake.h"
    "include/clither/thread.h"
    "include/clither/tick.h"
    "include/clither/tests.h"
    "include/clither/world.h"

    "src/args.c"
    "src/bezier.c"
    "src/camera.c"
    "src/client.c"
    "src/controls.c"
    "src/event.c"
    "src/log.c"
    "src/main.c"
    "src/msg.c"
    "src/msg_queue.c"
    "src/server.c"
    "src/server_settings.c"
    "src/snake.c"
    "src/world.c"

    $<$<STREQUAL:${CLITHER_GFX},SDL>:
        src/gfx_sdl.c>

    $<$<PLATFORM_ID:Windows>:
        src/mutex_win32.c
        src/net_win32.c
        src/signals_win32.c
        src/thread_win32.c
        src/tick_win32.c
        src/winmain.c>
    $<$<PLATFORM_ID:Linux>:
        src/mutex_pthread.c
        src/net_posix.c
        src/signals_posix.c
        src/thread_pthread.c
        src/tick_posix.c>
    $<$<PLATFORM_ID:Darwin>:
        src/mutex_pthread.c
        src/net_posix.c
        src/signals_posix.c
        src/thread_pthread.c
        src/tick_posix.c>

    $<$<BOOL:${CLITHER_TESTS}>:
        tests/tests.cpp
        tests/clither/test_args.cpp
        tests/clither/test_protocol.cpp
        tests/clither/test_tick.cpp
        tests/cstructures/test_btree_as_set.cpp
        tests/cstructures/test_btree.cpp
        tests/cstructures/test_hashmap.cpp
        tests/cstructures/test_vector.cpp>

    $<$<BOOL:${CLITHER_BENCHMARKS}>:
        benchmarks/benchmarks.cpp
        benchmarks/cstructures/bench_hashmap.cpp
        benchmarks/cstructures/bench_std_unordered_map.cpp
        benchmarks/cstructures/bench_std_vector.cpp
        benchmarks/cstructures/bench_vector.cpp>)
target_include_directories (clither
    PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_compile_definitions (clither
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
target_compile_options (clither
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/W4 /wd4706 /wd4305 /wd4244>
        $<$<C_COMPILER_ID:GNU>:-W -Wall -Wextra -pedantic>
        $<$<C_COMPILER_ID:Clang>:-W -Wall -Wextra -pedantic>)
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

add_subdirectory ("cstructures")

target_link_libraries (clither
    PRIVATE
        cstructures
        $<$<PLATFORM_ID:Windows>:ws2_32>)

find_package(Threads REQUIRED)
target_link_libraries (clither PRIVATE Threads::Threads)

if (UNIX)
    target_link_libraries (clither PRIVATE m)
endif ()

if (CLITHER_GFX STREQUAL "SDL")
    find_package (SDL2 REQUIRED)
    find_package (SDL2_image REQUIRED)
    target_link_libraries (clither
        PRIVATE
            SDL2::Core
            SDL2::Image)

    if (WIN32 OR CYGWIN)
        add_custom_command (TARGET clither POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SDL2_PATH}/lib/$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,x64,x86>/SDL2.dll ${CLITHER_BUILD_BINDIR}/SDL2.dll
            COMMENT "Copying SDL2 DLLs to bin directory"
            VERBATIM)
    endif ()
endif ()

if (CLITHER_TESTS)
    enable_language (CXX)
    add_subdirectory ("thirdparty/googletest")
    target_link_libraries (clither PRIVATE gmock)
endif ()

if (CLITHER_BENCHMARKS)
    enable_language (CXX)
    add_subdirectory ("thirdparty/benchmark")
    target_link_libraries (clither PRIVATE benchmark)
endif ()

###############################################################################
# Install targets
###############################################################################

install (
    TARGETS clither
    RUNTIME DESTINATION ${CLITHER_INSTALL_BINDIR})
