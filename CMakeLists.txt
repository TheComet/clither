cmake_minimum_required (VERSION 3.22)

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

cmake_dependent_option (CLITHER_BACKTRACE "Enable generating backtraces to every malloc/realloc call, making it easy to find where memory leaks occur" ON "CLITHER_MEMORY_DEBUGGING" OFF)
set (CLITHER_BACKTRACE_SIZE "64" CACHE STRING "Sets the maximum stack size (depth) when generating backtraces")
cmake_dependent_option (CLITHER_HEX_DUMP "Enable printing out hex dumps of the unfreed memory regions" OFF "CLITHER_MEMORY_DEBUGGING" OFF)
option (CLITHER_BENCHMARKS "Compile benchmarks (requires C++)" OFF)
cmake_dependent_option (CLITHER_BOT_API "Enable the bot API" ON "CLITHER_CLIENT" OFF)
option (CLITHER_CLIENT "Build the client component. This is required for graphics, or for running bots." ON)
option (CLITHER_DEBUG "Enable debug checks" ON)
cmake_dependent_option (CLITHER_DEBUG_ASSERT "Enable the CLITHER_DEBUG_ASSERT macro" ON "CLITHER_DEBUG" OFF)
cmake_dependent_option (CLITHER_DEBUG_ZOOM "Enable camera zooming using the mouse wheel" ON "CLITHER_DEBUG" OFF)
option (CLITHER_DOC "Compile LaTeX documents" ON)
cmake_dependent_option (CLITHER_GFX "Enable graphics frontends. Turn this off if you only care about running a server, or running bots." OFF "CLITHER_CLIENT" ON)
cmake_dependent_option (CLITHER_GFX_SDL "Enable SDL graphics backend" ON "CLITHER_GFX" OFF)
cmake_dependent_option (CLITHER_GFX_GLES2 "Enable OpenGL ES 2.0 graphics backend" ON "CLITHER_GFX" OFF)
cmake_dependent_option (CLITHER_GFX_VULKAN "Enable Vulkan graphics backend" OFF "CLITHER_GFX" OFF)
option (CLITHER_LOGGING "Enable logging functionality" ON)
option (CLITHER_MCD "Add McDonald's WiFi support (network latency and packet loss simulation)" ON)
option (CLITHER_MEMORY_DEBUGGING "Enable tracking malloc/realloc/free calls to detect memory leaks" ON)
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

check_c_source_compiles ("int main(void) { _Static_assert(1, \"\"); return 0; }" GCC_STATIC_ASSERT)
check_c_source_compiles ("int main(void) { _STATIC_ASSERT(1); return 0; }" MSVC_STATIC_ASSERT)
if (MSVC_STATIC_ASSERT)
    set (CLITHER_STATIC_ASSERT "_STATIC_ASSERT(condition)")
elseif (GCC_STATIC_ASSERT)
    set (CLITHER_STATIC_ASSERT "_Static_assert(condition, \"(\" #condition \") failed\")")
else ()
    set (CLITHER_STATIC_ASSERT "")
endif ()

configure_file ("templates/config.h.in" "include/clither/config.h")

###############################################################################
# Game source files
###############################################################################

add_executable (clither
    "templates/config.h.in"
    "${PROJECT_BINARY_DIR}/include/clither/config.h"

    "include/clither/benchmarks.h"
    "include/clither/tests.h"

    "include/clither/client/client.h"

    "include/clither/game/args.h"
    "include/clither/game/bezier.h"
    "include/clither/game/bezier_knot_rb.h"
    "include/clither/game/bezier_point_vec.h"
    "include/clither/game/camera.h"
    "include/clither/game/cmd.h"
    "include/clither/game/cmd_queue.h"
    "include/clither/game/cmd_rb.h"
    "include/clither/game/food.h"
    "include/clither/game/input.h"
    "include/clither/game/mcd_wifi.h"
    "include/clither/game/msg.h"
    "include/clither/game/msg_vec.h"
    "include/clither/game/q.h"
    "include/clither/game/qwaabb_rb.h"
    "include/clither/game/qwpos_vec.h"
    "include/clither/game/qwpos_vec_rb.h"
    "include/clither/game/resource_pack.h"
    "include/clither/game/resource_snake_part_vec.h"
    "include/clither/game/resource_sprite_vec.h"
    "include/clither/game/settings.h"
    "include/clither/game/snake.h"
    "include/clither/game/snake_bmap.h"
    "include/clither/game/snake_param.h"
    "include/clither/game/snake_split_rb.h"
    "include/clither/game/world.h"
    "include/clither/game/wrap.h"

    "include/clither/gfx/gfx.h"

    "include/clither/platform/backtrace.h"
    "include/clither/platform/fs.h"
    "include/clither/platform/mfile.h"
    "include/clither/platform/mutex.h"
    "include/clither/platform/net.h"
    "include/clither/platform/signals.h"
    "include/clither/platform/system.h"
    "include/clither/platform/thread.h"
    "include/clither/platform/tick.h"
    "include/clither/platform/utf8.h"

    "include/clither/util/bmap.h"
    "include/clither/util/bset.h"
    "include/clither/util/cli_colors.h"
    "include/clither/util/hash.h"
    "include/clither/util/hmap.h"
    "include/clither/util/hset.h"
    "include/clither/util/log.h"
    "include/clither/util/mem.h"
    "include/clither/util/morton.h"
    "include/clither/util/popcount.h"
    "include/clither/util/rb.h"
    "include/clither/util/str.h"
    "include/clither/util/strspan.h"
    "include/clither/util/strview.h"
    "include/clither/util/strlist.h"

    "include/clither/server/bezier_knot_acks_bmap.h"
    "include/clither/server/food_in_proximity_hset.h"
    "include/clither/server/net_addr_hmap.h"
    "include/clither/server/snakes_in_proximity_bmap.h"
    "include/clither/server/server.h"
    "include/clither/server/server_client.h"
    "include/clither/server/server_client_hmap.h"
    "include/clither/server/server_instance.h"
    "include/clither/server/server_instance_bmap.h"

    "src/game/args.c"
    "src/game/bezier.c"
    "src/game/bezier_knot_rb.c"
    "src/game/bezier_point_vec.c"
    "src/game/camera.c"
    "src/game/cmd.c"
    "src/game/cmd_queue.c"
    "src/game/cmd_rb.c"
    "src/game/food.c"
    "src/game/input.c"
    "src/game/msg.c"
    "src/game/msg_vec.c"
    "src/game/qwaabb_rb.c"
    "src/game/qwpos_vec.c"
    "src/game/qwpos_vec_rb.c"
    "src/game/resource_pack.c"
    "src/game/resource_snake_part_vec.c"
    "src/game/resource_sprite_vec.c"
    "src/game/settings.c"
    "src/game/snake.c"
    "src/game/snake_bmap.c"
    "src/game/snake_param.c"
    "src/game/snake_split_rb.c"
    "src/game/world.c"

    "src/util/hash.c"
    "src/util/log.c"
    "src/util/morton.c"
    "src/util/str.c"
    "src/util/strview.c"
    "src/util/strlist.c"

    $<$<BOOL:${CLITHER_SERVER}>:
        src/server/bezier_knot_acks_bmap.c
        src/server/snakes_in_proximity_bmap.c
        src/server/food_in_proximity_hset.c
        src/server/net_addr_hmap.c
        src/server/server.c
        src/server/server_client.c
        src/server/server_client_hmap.c
        src/server/server_instance.c
        src/server/server_instance_bmap.c>

    $<$<BOOL:${CLITHER_MCD}>:
        src/game/mcd_wifi.c>

    $<$<BOOL:${CLITHER_CLIENT}>:
        src/client/client.c>

    $<$<BOOL:${CLITHER_GFX}>:
        src/gfx/gfx.c
        $<$<BOOL:${CLITHER_GFX_SDL}>:
            src/gfx/sdl/gfx_sdl.c>
        $<$<BOOL:${CLITHER_GFX_GLES2}>:
            src/gfx/gles2/gfx_gles2.c
            src/gfx/gles2/internal/background.c
            src/gfx/gles2/internal/food.c
            src/gfx/gles2/internal/quad.c
            src/gfx/gles2/internal/shader.c
            src/gfx/gles2/internal/snake.c
            src/gfx/gles2/internal/sprite.c
            src/gfx/gles2/internal/sprite_shadow.c
            src/gfx/gles2/internal/text.c
            src/gfx/gles2/glad/src/gles2.c>
        $<$<BOOL:${CLITHER_GFX_VULKAN}>:
            src/gfx/vulkan/gfx_vulkan.c>>

    $<$<BOOL:${CLITHER_MEMORY_DEBUGGING}>:
        src/util/mem.c>

    $<$<PLATFORM_ID:Windows>:
        $<$<BOOL:${CLITHER_BACKTRACE}>:src/backtrace_win32.c>
        src/platform/fs_win32.c
        src/platform/mfile_win32.c
        src/platform/mutex_win32.c
        src/platform/net_win32.c
        src/platform/signals_win32.c
        src/platform/system_win32.c
        src/platform/thread_win32.c
        src/platform/tick_win32.c
        src/platform/utf8_win32.c
        src/winmain.c>
    $<$<PLATFORM_ID:Linux>:
        $<$<BOOL:${CLITHER_BACKTRACE}>:src/backtrace_linux.c>
        src/platform/fs_linux.c
        src/platform/mfile_posix.c
        src/platform/mutex_pthread.c
        src/platform/net_posix.c
        src/platform/signals_posix.c
        src/platform/thread_pthread.c
        src/platform/tick_posix.c
        src/platform/utf8_posix.c>
    $<$<PLATFORM_ID:Darwin>:
        src/platform/fs_linux.c
        src/platform/mfile_posix.c
        src/platform/mutex_pthread.c
        src/platform/net_posix.c
        src/platform/signals_posix.c
        src/platform/thread_pthread.c
        src/platform/tick_posix.c
        src/platform/utf8_posix.c>
    $<$<PLATFORM_ID:Emscripten>:
        src/platform/fs_linux.c
        src/platform/net_em.c
        src/platform/signals_em.c
        src/platform/tick_em.c
        src/platform/utf8_posix.c>

    $<$<BOOL:${CLITHER_TESTS}>:
        tests/tests.cpp
        tests/src/LogHelper.cpp
        tests/clither/test_args.cpp
        tests/clither/test_bezier_aabb.cpp
        tests/clither/test_bezier_fit.cpp
        tests/clither/test_bezier_point.cpp
        tests/clither/test_cmd.cpp
        tests/clither/test_hmap.cpp
        tests/clither/test_hmap_full.cpp
        tests/clither/test_hset.cpp
        tests/clither/test_mem.cpp
        tests/clither/test_morton.cpp
        tests/clither/test_msg.cpp
        tests/clither/test_q.cpp
        tests/clither/test_rb.cpp
        tests/clither/test_snake.cpp
        tests/clither/test_tick.cpp
        tests/clither/test_vec.cpp
        tests/clither/test_wrap.cpp
        tests/clither/test_bmap.cpp
        tests/clither/test_bset.cpp
        $<$<AND:$<BOOL:${CLITHER_CLIENT}>,$<BOOL:${CLITHER_SERVER}>>:
            tests/clither/test_protocol_feedback.cpp
            tests/clither/test_protocol_join.cpp>>

    $<$<BOOL:${CLITHER_BENCHMARKS}>:
        benchmarks/benchmarks.cpp
        benchmarks/clither/bench_hashmap.cpp
        benchmarks/clither/bench_std_unordered_map.cpp
        benchmarks/clither/bench_std_vector.cpp
        benchmarks/clither/bench_vec.cpp>

        "src/main.c")
target_include_directories (clither
    PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
        
        $<$<BOOL:${CLITHER_TESTS}>:
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tests/include>>

        $<$<BOOL:${CLITHER_GFX_GLES2}>:
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/gfx/gles2/glad/include>>)
target_compile_definitions (clither
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
target_compile_options (clither
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/W4 /wd4706 /wd4305 /wd4244 /wd4505>
        $<$<C_COMPILER_ID:GNU>:-W -Wall -Wextra -pedantic -Wno-unused-function -Wno-overlength-strings>
        $<$<C_COMPILER_ID:Clang>:-W -Wall -Wextra -pedantic -Wno-unused-function>)
target_link_options (clither
    PRIVATE
        $<$<BOOL:${CLITHER_BACKTRACE}>:
            $<$<C_COMPILER_ID:Clang>:-rdynamic>
            $<$<C_COMPILER_ID:GNU>:-rdynamic>>)
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

target_link_libraries (clither
    PRIVATE
        $<$<PLATFORM_ID:Windows>:
            ws2_32
            $<$<BOOL:${CLITHER_BACKTRACE}>:Dbghelp>>
        $<$<PLATFORM_ID:Linux>:m>
        $<$<PLATFORM_ID:Darwin>:m>
        $<$<PLATFORM_ID:Emscripten>:websocket.js>)

find_package(Threads REQUIRED)
target_link_libraries (clither PRIVATE Threads::Threads)

if (CLITHER_GFX)
    set (FT_DISABLE_HARFBUZZ ON CACHE STRING "")
    add_subdirectory ("thirdparty/freetype-2.13.3")
    target_link_libraries (clither PRIVATE freetype)

    set (HB_BUILD_SUBSET ON CACHE STRING "")
    add_subdirectory ("thirdparty/harfbuzz-11.0.0")
    target_link_libraries (clither PRIVATE harfbuzz)
endif ()

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
