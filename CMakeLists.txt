cmake_minimum_required (VERSION 3.24)

if (NOT DEFINED CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif()
message (STATUS "Build type: ${CMAKE_BUILD_TYPE}")

project ("clither"
    VERSION 0.0.1
    LANGUAGES C)

###############################################################################
# Config and options
###############################################################################

include (CMakeDependentOption)
option (CLITHER_BENCHMARKS "Compile benchmarks (requires C++)" ON)
option (CLITHER_DOC "Compile LaTeX documents" ON)
set (CLITHER_GFX "GLES2" CACHE STRING "Set to an empty string or OFF if you only want to build a server (headless) application.")
set_property (CACHE CLITHER_GFX PROPERTY STRINGS "OFF;SDL;GLES2")
option (CLITHER_LOGGING "Enable logging functionality" ON)
option (CLITHER_MCD "Add McDonald's WiFi support (network latency and packet loss simulation)" ON)
option (CLITHER_TESTS "Compile unit tests (requires C++)" ON)

set (CLITHER_BUILD_BINDIR "${PROJECT_BINARY_DIR}/bin")
set (CLITHER_BUILD_LIBDIR "${CLITHER_BUILD_BINDIR}/lib")
set (CLITHER_BUILD_DOCDIR "${CLITHER_BUILD_BINDIR}/doc")

set (CLITHER_INSTALL_BINDIR ".")
set (CLITHER_INSTALL_LIBDIR "lib")
set (CLITHER_INSTALL_DOCDIR "doc")

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
    "include/clither/food_cluster.h"
    "include/clither/gfx.h"
    "include/clither/log.h"
    "include/clither/mcd_wifi.h"
    "include/clither/msg.h"
    "include/clither/msg_queue.h"
    "include/clither/mutex.h"
    "include/clither/net.h"
    "include/clither/q.h"
    "include/clither/server.h"
    "include/clither/server_settings.h"
    "include/clither/signals.h"
    "include/clither/snake.h"
    "include/clither/snake_head.h"
    "include/clither/thread.h"
    "include/clither/tick.h"
    "include/clither/tests.h"
    "include/clither/world.h"
    "include/clither/wrap.h"

    "src/args.c"
    "src/bezier.c"
    "src/camera.c"
    "src/client.c"
    "src/controls.c"
    "src/event.c"
    "src/food_cluster.c"
    "src/log.c"
    "src/main.c"
    "src/mcd_wifi.c"
    "src/msg.c"
    "src/msg_queue.c"
    "src/server.c"
    "src/server_settings.c"
    "src/snake.c"
    "src/world.c"

    $<$<STREQUAL:${CLITHER_GFX},SDL>:
        src/sdl/gfx_sdl.c>

    $<$<STREQUAL:${CLITHER_GFX},GLES2>:
        src/gles2/gfx_gles2.c
        src/gles2/glad/src/gles2.c>

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
        tests/clither/test_bezier.cpp
        tests/clither/test_msg.cpp
        tests/clither/test_protocol.cpp
        tests/clither/test_q.cpp
        tests/clither/test_quadtree.cpp
        tests/clither/test_snake.cpp
        tests/clither/test_tick.cpp
        tests/clither/test_wrap.cpp
        tests/cstructures/test_btree_as_set.cpp
        tests/cstructures/test_btree.cpp
        tests/cstructures/test_hashmap.cpp
        tests/cstructures/test_memory.cpp
        tests/cstructures/test_rb.cpp
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
        $<INSTALL_INTERFACE:include>
        
        $<$<STREQUAL:${CLITHER_GFX},GLES2>:
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/gles2/glad/include>>)
target_compile_definitions (clither
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
target_compile_options (clither
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/W4 /wd4706 /wd4305 /wd4244>
        $<$<C_COMPILER_ID:GNU>:-W -Wall -Wextra -pedantic -Wno-unused-function>
        $<$<C_COMPILER_ID:Clang>:-W -Wall -Wextra -pedantic -Wno-unused-function>)
set_target_properties (clither
    PROPERTIES
        C_STANDARD 90
        VS_DEBUGGER_WORKING_DIRECTORY ${CLITHER_BUILD_BINDIR}
        VS_DEBUGGER_COMMAND_ARGUMENTS "--host"
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
    set (SDL2_DISABLE_INSTALL ON CACHE STRING "")
    set (SDL2_DISABLE_SDL2MAIN ON CACHE STRING "")
    set (SDL2_DISABLE_UNINSTALL ON CACHE STRING "")
    set (SDL_SHARED OFF CACHE STRING "")
    set (SDL_STATIC ON CACHE STRING "")
    set (SDL_TEST OFF CACHE STRING "")
    add_subdirectory ("thirdparty/SDL-2.26.5")
    target_link_libraries (clither PRIVATE SDL2-static)

    set (BUILD_SHARED_LIBS OFF CACHE STRING "")
    set (SDL2IMAGE_INSTALL OFF CACHE STRING "")
    set (SDL2IMAGE_SAMPLES OFF CACHE STRING "")
    add_subdirectory ("thirdparty/SDL_image-2.6.3")
    target_link_libraries (clither PRIVATE SDL2_image)
elseif (CLITHER_GFX STREQUAL "GLES2")
    set (GLFW_BUILD_EXAMPLES OFF CACHE STRING "")
    set (GLFW_BUILD_TESTS OFF CACHE STRING "")
    set (GLFW_BUILD_DOCS OFF CACHE STRING "")
    set (GLFW_INSTALL OFF CACHE STRING "")
    add_subdirectory ("thirdparty/glfw-3.3.8")
    target_link_libraries (clither PRIVATE glfw)
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

if (CLITHER_DOC)
    add_subdirectory ("doc/clither-doc")
endif ()

###############################################################################
# Install targets
###############################################################################

install (
    TARGETS clither
    RUNTIME DESTINATION ${CLITHER_INSTALL_BINDIR})
