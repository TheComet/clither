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
option (CLITHER_GFX "Set to OFF if you only want to build a server (headless) application.")
cmake_dependent_option (CLITHER_GFX_SDL "Enable SDL graphics backend" ON "CLITHER_GFX" OFF)
cmake_dependent_option (CLITHER_GFX_GLES2 "Enable OpenGL ES 2.0 graphics backend" ON "CLITHER_GFX" OFF)
cmake_dependent_option (CLITHER_GFX_VULKAN "Enable Vulkan graphics backend" OFF "CLITHER_GFX" OFF)
option (CLITHER_LOGGING "Enable logging functionality" ON)
option (CLITHER_MCD "Add McDonald's WiFi support (network latency and packet loss simulation)" ON)
option (CLITHER_SERVER "Build the server component. Requires threads." ON)
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
elseif (GCC_THREADLOCAL)
    set (CLITHER_THREADLOCAL "__thread")
else ()
    message (FATAL_ERROR "Unable to detect thread local storage semantics!")
endif ()

check_c_source_compiles ("int main(void) { return __popcnt(0xAAAA); }" MSVC_POPCOUNT)
check_c_source_compiles ("int main(void) { return __builtin_popcount(0xAAAA); }" GCC_POPCOUNT)
if (MSVC_POPCOUNT)
    set (CLITHER_POPCOUNT "__popcnt")
elseif (GCC_POPCOUNT)
    set (CLITHER_POPCOUNT "__builtin_popcount")
else ()
    message (WARNING "Could not detect a popcount intrinsic, falling back to slow implementation")
    set (CLITHER_POPCOUNT OFF)
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
    "include/clither/command.h"
    "include/clither/food_cluster.h"
    "include/clither/fs.h"
    "include/clither/gfx.h"
    "include/clither/input.h"
    "include/clither/log.h"
    "include/clither/mcd_wifi.h"
    "include/clither/msg.h"
    "include/clither/msg_queue.h"
    "include/clither/mutex.h"
    "include/clither/net.h"
    "include/clither/popcount.h"
    "include/clither/q.h"
    "include/clither/resource_pack.h"
    "include/clither/server.h"
    "include/clither/server_settings.h"
    "include/clither/signals.h"
    "include/clither/snake.h"
    "include/clither/snake_param.h"
    "include/clither/thread.h"
    "include/clither/tick.h"
    "include/clither/tests.h"
    "include/clither/utf8.h"
    "include/clither/world.h"
    "include/clither/wrap.h"

    "src/args.c"
    "src/bezier.c"
    "src/camera.c"
    "src/client.c"
    "src/command.c"
    "src/food_cluster.c"
    "src/gfx.c"
    "src/input.c"
    "src/log.c"
    "src/main.c"
    "src/msg.c"
    "src/msg_queue.c"
    "src/resource_pack.c"
    "src/snake.c"
    "src/snake_param.c"
    "src/world.c"

    $<$<BOOL:${CLITHER_SERVER}>:
        src/server.c
        src/server_settings.c>

    $<$<BOOL:${CLITHER_MCD}>:
        src/mcd_wifi.c>

    $<$<BOOL:${CLITHER_GFX_SDL}>:
        src/sdl/gfx_sdl.c>

    $<$<BOOL:${CLITHER_GFX_GLES2}>:
        src/gles2/gfx_gles2.c
        src/gles2/glad/src/gles2.c>

    $<$<BOOL:${CLITHER_GFX_VULKAN}>:
        src/vulkan/gfx_vulkan.c>

    $<$<PLATFORM_ID:Windows>:
        src/fs_win32.c
        src/mutex_win32.c
        src/net_win32.c
        src/signals_win32.c
        src/thread_win32.c
        src/tick_win32.c
        src/utf8_win32.c
        src/winmain.c>
    $<$<PLATFORM_ID:Linux>:
        src/fs_posix.c
        src/mutex_pthread.c
        src/net_posix.c
        src/signals_posix.c
        src/thread_pthread.c
        src/tick_posix.c
        src/utf8_posix.c>
    $<$<PLATFORM_ID:Darwin>:
        src/fs_posix.c
        src/mutex_pthread.c
        src/net_posix.c
        src/signals_posix.c
        src/thread_pthread.c
        src/tick_posix.c
        src/utf8_posix.c>
    $<$<PLATFORM_ID:Emscripten>:
        src/fs_posix.c
        src/net_em.c
        src/signals_em.c
        src/tick_em.c
        src/utf8_posix.c>

    $<$<BOOL:${CLITHER_TESTS}>:
        tests/tests.cpp
        tests/clither/test_args.cpp
        tests/clither/test_bezier.cpp
        tests/clither/test_command.cpp
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

        $<$<BOOL:${CLITHER_GFX_GLES2}>:
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/gles2/glad/include>>)
target_compile_definitions (clither
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
target_compile_options (clither
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/W4 /wd4706 /wd4305 /wd4244>
        $<$<C_COMPILER_ID:GNU>:-W -Wall -Wextra -pedantic -Wno-unused-function -Wno-overlength-strings>
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

if (EMSCRIPTEN)
    set_target_properties(clither
        PROPERTIES
            SUFFIX ".html"
            LINK_FLAGS "-sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH -sASYNCIFY --preload-file ${CLITHER_BUILD_BINDIR}/packs@/packs --bind")
endif ()

###############################################################################
# Dependencies
###############################################################################

add_subdirectory ("cstructures")

target_link_libraries (clither
    PRIVATE
        cstructures
        $<$<PLATFORM_ID:Windows>:ws2_32>
        $<$<PLATFORM_ID:Linux>:m>
        $<$<PLATFORM_ID:Darwin>:m>
        $<$<PLATFORM_ID:Emscripten>:websocket.js>)

find_package(Threads REQUIRED)
target_link_libraries (clither PRIVATE Threads::Threads)

if (CLITHER_GFX_SDL)
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
endif ()

if (CLITHER_GFX_GLES2)
    # Emscripten ships with GLFW already
    if (NOT EMSCRIPTEN)
        set (GLFW_BUILD_EXAMPLES OFF CACHE STRING "")
        set (GLFW_BUILD_TESTS OFF CACHE STRING "")
        set (GLFW_BUILD_DOCS OFF CACHE STRING "")
        set (GLFW_INSTALL OFF CACHE STRING "")
        add_subdirectory ("thirdparty/glfw-3.3.8")
        target_link_libraries (clither PRIVATE glfw)
    endif ()

    target_include_directories (clither PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/thirdparty/stb>)
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
