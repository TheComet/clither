cmake_minimum_required (VERSION 3.22)

if (NOT DEFINED CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif()
message (STATUS "Build type: ${CMAKE_BUILD_TYPE}")

project ("clither"
    VERSION 0.0.1
    LANGUAGES C)

list (APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")

###############################################################################
# Config and options
###############################################################################

include (CMakeDependentOption)

option (CLITHER_BENCHMARKS "Compile benchmarks (requires C++)" OFF)
option (CLITHER_CAPACITY_WARNING "Enable capacity warnings for data structures" OFF)
option (CLITHER_DEBUG_ASSERT "Enable the CLITHER_DEBUG_ASSERT macro" OFF)
option (CLITHER_DEBUG_ZOOM "Enable camera zooming using the mouse wheel" OFF)
option (CLITHER_DOC "Compile LaTeX documents" ON)
option (CLITHER_FETCH_ASSETS "Download and install assets automatically" ON)
option (CLITHER_HOT_RELOAD "Enable hot-reloading of the game assets" ON)
option (CLITHER_MCD "Add McDonald's WiFi support (network latency and packet loss simulation)" OFF)
option (CLITHER_LOG "Enable logging functionality" ON)
option (CLITHER_LOG_DEBUG "Enable log_dbg()" OFF)
option (CLITHER_TESTS "Compile unit tests (requires C++)" ON)
option (CLITHER_WEB_MENU "Optimize main menu for the web build" OFF)

set (CLITHER_ASM_OPTIMIZATIONS "HOTPATCH" CACHE STRING "")
set_property (CACHE CLITHER_ASM_OPTIMIZATIONS
    PROPERTY STRINGS "OFF;HARDCODE;HOTPATCH;FUNCPTR")

option (CLITHER_AUDIO "Enable audio support" ON)
cmake_dependent_option (CLITHER_AUDIO_OPENAL "Enable OpenAL audio backend" ON "CLITHER_AUDIO" OFF)

option (CLITHER_CLIENT "Build the client component. This is required for graphics, or for running bots." ON)
cmake_dependent_option (CLITHER_BOT_API "Enable the bot API" ON "CLITHER_CLIENT" OFF)
cmake_dependent_option (CLITHER_BOT_API_LUA "Enable lua bindings for the bot API" ON "CLITHER_BOT_API" OFF)
cmake_dependent_option (CLITHER_GFX "Enable graphics frontends. Turn this off if you only care about running a server, or running bots." ON "CLITHER_CLIENT" OFF)
cmake_dependent_option (CLITHER_GFX_DEBUG "Enable the graphics debug layer" ON "CLITHER_GFX" OFF)
cmake_dependent_option (CLITHER_GFX_GLES2 "Enable OpenGL ES 2.0 graphics backend" ON "CLITHER_GFX" OFF)
cmake_dependent_option (CLITHER_GFX_SDL "Enable SDL graphics backend" OFF "CLITHER_GFX" OFF)
cmake_dependent_option (CLITHER_GFX_VULKAN "Enable Vulkan graphics backend" OFF "CLITHER_GFX" OFF)

option (CLITHER_DEBUG_MEMORY "Enable tracking malloc/realloc/free calls to detect memory leaks" OFF)
cmake_dependent_option (CLITHER_BACKTRACE "Enable generating backtraces to every malloc/realloc call, making it easy to find where memory leaks occur" OFF "CLITHER_DEBUG_MEMORY" OFF)
cmake_dependent_option (CLITHER_HEX_DUMP "Enable printing out hex dumps of the unfreed memory regions" OFF "CLITHER_DEBUG_MEMORY" OFF)
set (CLITHER_HEX_DUMP_SIZE "256" CACHE STRING "Maximum size in bytes of the hex dump of the unfreed memory regions")
set (CLITHER_BACKTRACE_SIZE "64" CACHE STRING "Sets the maximum stack size (depth) when generating backtraces")

option (CLITHER_SERVER "Build the server component. Requires threads." ON)
cmake_dependent_option (CLITHER_SERVER_WEBSOCKETS "Enable WebSocket support" ON "CLITHER_SERVER" OFF)

set (CLITHER_BUILD_BINDIR "${PROJECT_BINARY_DIR}/bin")
set (CLITHER_BUILD_LIBDIR "${CLITHER_BUILD_BINDIR}/lib")
set (CLITHER_BUILD_DOCDIR "${CLITHER_BUILD_BINDIR}/doc")

set (CLITHER_INSTALL_BINDIR ".")
set (CLITHER_INSTALL_LIBDIR "lib")
set (CLITHER_INSTALL_DOCDIR "doc")

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
    set (CLITHER_THREADLOCAL "__declspec(thread)")
    set (CLITHER_STATIC_ASSERT "_STATIC_ASSERT(condition)")
    set (CLITHER_HOTPATCH "__declspec(noinline)")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    set (CLITHER_THREADLOCAL "__thread")
    #set (CLITHER_STATIC_ASSERT "_Static_assert(condition, \"(\" #condition \") failed\")")
    set (CLITHER_STATIC_ASSERT "{                \\\n        typedef char static_assertion[(condition) ? 1 : -1]; \\\n        static_assertion static_assertion_;                  \\\n        (void)static_assertion_;                             \\\n    }")
    set (CLITHER_HOTPATCH "__attribute__((noinline)) __attribute__((noclone))")
else ()
    message (FATAL_ERROR "Builtins not yet implemented for compiler: ${CMAKE_C_COMPILER_ID}\nYou could try to add them yourself (just check CMakeLists.txt at this location)")
endif ()

include (CheckCSourceCompiles)
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

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set (CLITHER_DEBUG ON)
endif ()

configure_file ("templates/config.h.in" "include/clither/config.h")

###############################################################################
# Game source files
###############################################################################

list (APPEND ClitherSources
    "templates/config.h.in"
    "${PROJECT_BINARY_DIR}/include/clither/config.h"

    "include/clither/benchmarks.h"
    "include/clither/tests.h"

    "include/clither/bot/bot.h"

    "include/clither/client/client.h"

    "include/clither/game/args.h"
    "include/clither/game/bezier.h"
    "include/clither/game/bezier_knot_rb.h"
    "include/clither/game/bezier_segment_rb.h"
    "include/clither/game/camera.h"
    "include/clither/game/cmd.h"
    "include/clither/game/cmd_queue.h"
    "include/clither/game/cmd_rb.h"
    "include/clither/game/food.h"
    "include/clither/game/input.h"
    "include/clither/game/mcd_wifi.h"
    "include/clither/game/msg.h"
    "include/clither/game/msg_vec.h"
    "include/clither/game/projectile.h"
    "include/clither/game/q.h"
    "include/clither/game/qwaabb_rb.h"
    "include/clither/game/qwpos_vec.h"
    "include/clither/game/qwpos_vec_rb.h"
    "include/clither/game/resource_pack.h"
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
    "include/clither/platform/semaphore.h"
    "include/clither/platform/system.h"
    "include/clither/platform/thread.h"
    "include/clither/platform/tick.h"
    "include/clither/platform/asm_optimizations.h"
    "include/clither/platform/utf8.h"

    "include/clither/util/base64.h"
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
    "include/clither/util/sha1.h"
    "include/clither/util/str.h"
    "include/clither/util/strspan.h"
    "include/clither/util/strview.h"
    "include/clither/util/strlist.h"
    "include/clither/util/tracker.h"

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
    "src/game/bezier_segment_rb.c"
    "src/game/camera.c"
    "src/game/cmd.c"
    "src/game/cmd_queue.c"
    "src/game/cmd_rb.c"
    "src/game/food.c"
    "src/game/input.c"
    "src/game/msg.c"
    "src/game/msg_vec.c"
    "src/game/projectile.c"
    "src/game/qwaabb_rb.c"
    "src/game/qwpos_vec.c"
    "src/game/qwpos_vec_rb.c"
    "src/game/resource_pack.c"
    "src/game/settings.c"
    "src/game/snake.c"
    "src/game/snake_bmap.c"
    "src/game/snake_param.c"
    "src/game/snake_split_rb.c"
    "src/game/world.c"

    "src/util/base64.c"
    "src/util/hash.c"
    "src/util/log.c"
    "src/util/morton.c"
    "src/util/sha1.c"
    "src/util/str.c"
    "src/util/strview.c"
    "src/util/strlist.c")

if (CLITHER_ASM_OPTIMIZATIONS)
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
        enable_language (ASM)
        set (Flavor "gas")
        set (Ext "s")
    endif ()
    if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        enable_language (ASM_MASM)
        set (Flavor "masm")
        set (Ext "asm")
    endif ()
    if (WIN32)
        set (Abi "win64")
    endif ()
    if (UNIX)
        set (Abi "sysv64")
    endif ()

    list (APPEND ClitherSources
        "src/util/morton_${Flavor}_${Abi}.${Ext}"
        "src/util/morton_${Flavor}_${Abi}.${Ext}")

    if (CLITHER_ASM_OPTIMIZATIONS STREQUAL "FUNCPTR")
        list (APPEND ClitherSources
            "src/platform/asm_funcptrs_${Flavor}_${Abi}.${Ext}")
    endif ()
    if (CLITHER_ASM_OPTIMIZATIONS STREQUAL "HOTPATCH")
        list (APPEND ClitherSources
            "src/platform/asm_trampolines_${Flavor}_${Abi}.${Ext}")
    endif ()
endif ()

if (CLITHER_AUDIO)
    list (APPEND ClitherSources
        "src/audio/audio.c"
        "src/audio/audio_decoder.c"
        "src/audio/audio_decoder_flac.c")
    if (CLITHER_AUDIO_OPENAL)
        list (APPEND ClitherSources
            "src/audio/audio_openal.c")
    endif ()
endif ()

if (CLITHER_BOT_API)
    list (APPEND ClitherSources "src/bot/bot.c")
    if (CLITHER_BOT_API_LUA)
        list (APPEND ClitherSources "src/bot/lua/bot_lua.c")
    endif ()
endif ()

if (CLITHER_CLIENT)
    list (APPEND ClitherSources "src/client/client.c")
    if (NOT EMSCRIPTEN)
        list (APPEND ClitherSources "src/platform/net_udp_client.c")
    endif ()
endif ()

if (CLITHER_GFX)
    list (APPEND ClitherSources
        "src/gfx/gfx.c"
        "src/ui/ui.c"
        "src/ui/button.c"
        "src/ui/controller.c"
        "src/ui/element.c"
        "src/ui/garage_menu.c"
        "src/ui/main_menu.c"
        "src/ui/rectangle.c"
        "src/ui/slider.c"
        "src/ui/style.c"
        "src/ui/text.c"
        "src/ui/textinput.c")
    if (CLITHER_GFX_SDL)
        list (APPEND ClitherSources "src/gfx/sdl/gfx_sdl.c")
    endif ()
    if (CLITHER_GFX_GLES2)
        list (APPEND ClitherSources
            "src/gfx/gles2/gfx_gles2.c"
            "src/gfx/gles2/glad/src/gles2.c"
            "src/gfx/gles2/internal/background.c"
            "src/gfx/gles2/internal/food.c"
            "src/gfx/gles2/internal/quad.c"
            "src/gfx/gles2/internal/rectangle.c"
            "src/gfx/gles2/internal/shader.c"
            "src/gfx/gles2/internal/snake.c"
            "src/gfx/gles2/internal/spine.c"
            "src/gfx/gles2/internal/sprite.c"
            "src/gfx/gles2/internal/sprite_shadow.c"
            "src/gfx/gles2/internal/text.c")
        if (CLITHER_GFX_DEBUG)
            list (APPEND ClitherSources "src/gfx/gles2/internal/debug.c")
        endif ()
    endif ()
    if (CLITHER_GFX_VULKAN)
        list (APPEND ClitherSources "src/gfx/vulkan/gfx_vulkan.c")
    endif ()
endif ()

if (CLITHER_MCD)
    list (APPEND ClitherSources "src/game/mcd_wifi.c")
endif ()

if (CLITHER_DEBUG_MEMORY)
    list (APPEND ClitherSources
        "src/util/mem.c"
        "src/util/tracker.c")
endif ()

if (CLITHER_SERVER)
    list (APPEND ClitherSources
        "src/server/bezier_knot_acks_bmap.c"
        "src/server/snakes_in_proximity_bmap.c"
        "src/server/food_in_proximity_hset.c"
        "src/server/net_addr_hmap.c"
        "src/server/server.c"
        "src/server/server_client.c"
        "src/server/server_client_hmap.c"
        "src/server/server_instance.c"
        "src/server/server_instance_bmap.c"
        "src/platform/net_udp_server.c")
    if (CLITHER_SERVER_WEBSOCKETS)
        list (APPEND ClitherSources "src/platform/net_ws_server.c")
    endif ()
endif ()

if (WIN32)
    list (APPEND ClitherSources
        "src/platform/asm_optimizations_win32.c"
        "src/platform/fs_win32.c"
        "src/platform/mfile_win32.c"
        "src/platform/mutex_win32.c"
        "src/platform/net_win32.c"
        "src/platform/signals_win32.c"
        "src/platform/semaphore_win32.c"
        "src/platform/system_win32.c"
        "src/platform/thread_win32.c"
        "src/platform/tick_win32.c"
        "src/platform/utf8_win32.c"
        "src/winmain.c")
    if (CLITHER_BACKTRACE)
        list (APPEND ClitherSources "src/platform/backtrace_win32.c")
    endif ()
endif ()

if (LINUX)
    list (APPEND ClitherSources
        "src/platform/asm_optimizations_posix.c"
        "src/platform/fs_linux.c"
        "src/platform/mfile_posix.c"
        "src/platform/mutex_pthread.c"
        "src/platform/net_posix.c"
        "src/platform/signals_posix.c"
        "src/platform/semaphore_posix.c"
        "src/platform/system_linux.c"
        "src/platform/thread_pthread.c"
        "src/platform/tick_posix.c"
        "src/platform/utf8_posix.c")
    if (CLITHER_BACKTRACE)
        list (APPEND ClitherSources "src/platform/backtrace_linux.c")
    endif ()
endif ()

if (APPLE)
    list (APPEND ClitherSources
        "src/platform/asm_optimizations_posix.c"
        "src/platform/fs_linux.c"
        "src/platform/mfile_posix.c"
        "src/platform/mutex_pthread.c"
        "src/platform/net_posix.c"
        "src/platform/signals_posix.c"
        "src/platform/semaphore_posix.c"
        "src/platform/system_linux.c"
        "src/platform/thread_pthread.c"
        "src/platform/tick_posix.c"
        "src/platform/utf8_posix.c")
    if (CLITHER_BACKTRACE)
        list (APPEND ClitherSources "src/platform/backtrace_linux.c")
    endif ()
endif ()

if (EMSCRIPTEN)
    list (APPEND ClitherSources
        "src/platform/asm_optimizations_em.c"
        "src/platform/fs_em.c"
        "src/platform/mfile_posix.c"
        "src/platform/net_em.c"
        "src/platform/net_ws_client.c"
        "src/platform/signals_em.c"
        "src/platform/tick_em.c"
        "src/platform/utf8_posix.c")
endif ()

if (CLITHER_TESTS)
    list (APPEND ClitherSources
        "tests/tests.cpp"
        "tests/src/LogHelper.cpp"
        "tests/clither/test_args.cpp"
        "tests/clither/test_bezier_aabb.cpp"
        "tests/clither/test_bezier_fit.cpp"
        "tests/clither/test_bezier_sample.cpp"
        "tests/clither/test_bmap.cpp"
        "tests/clither/test_bset.cpp"
        "tests/clither/test_cmd.cpp"
        "tests/clither/test_hmap.cpp"
        "tests/clither/test_hmap_full.cpp"
        "tests/clither/test_hset.cpp"
        "tests/clither/test_mem.cpp"
        "tests/clither/test_morton.cpp"
        "tests/clither/test_msg.cpp"
        "tests/clither/test_q.cpp"
        "tests/clither/test_rb.cpp"
        "tests/clither/test_snake_rollback.cpp"
        "tests/clither/test_strlist.cpp"
        "tests/clither/test_strview.cpp"
        "tests/clither/test_tick.cpp"
        "tests/clither/test_vec.cpp"
        "tests/clither/test_wrap.cpp")
    if (CLITHER_CLIENT AND CLITHER_SERVER)
        list (APPEND ClitherSources
            "tests/clither/test_protocol_feedback.cpp"
            "tests/clither/test_protocol_join.cpp")
    endif ()
    if (CLITHER_SERVER_WEBSOCKETS)
        list (APPEND ClitherSources "tests/clither/test_ws.cpp")
    endif ()
endif ()

if (CLITHER_BENCHMARKS)
    list (APPEND ClitherSources
        "benchmarks/benchmarks.cpp"
        "benchmarks/clither/bench_hashmap.cpp"
        "benchmarks/clither/bench_std_unordered_map.cpp"
        "benchmarks/clither/bench_std_vector.cpp"
        "benchmarks/clither/bench_vec.cpp")
endif ()

###############################################################################
# Game executable
###############################################################################

add_executable (clither
    ${ClitherSources}
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
        CXX_STANDARD 17  # Required for LogHelper std::string::data() non-const version
        RUNTIME_OUTPUT_DIRECTORY ${CLITHER_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CLITHER_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CLITHER_BUILD_BINDIR}
        INSTALL_RPATH ${CLITHER_INSTALL_LIBDIR}
        MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>
        VS_DEBUGGER_WORKING_DIRECTORY ${CLITHER_BUILD_BINDIR}
        #VS_DEBUGGER_COMMAND_ARGUMENTS ""
        OUTPUT_NAME "mechasnek")
set_property (DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY
    VS_STARTUP_PROJECT clither)

# Visual studio should show the source files in their original hierarchy
get_target_property (source_list clither SOURCES)
source_group (TREE ${PROJECT_SOURCE_DIR} FILES ${source_list})

install (
    TARGETS clither
    RUNTIME DESTINATION ${CLITHER_INSTALL_BINDIR})

if (CLITHER_BOT_API_LUA)
    add_custom_command (
        TARGET clither
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${PROJECT_SOURCE_DIR}/lua"
            "${CLITHER_BUILD_BINDIR}/lua"
        COMMENT "Copying lua files to build directory")
    install (
        DIRECTORY "${PROJECT_SOURCE_DIR}/lua/"
        DESTINATION "${CLITHER_INSTALL_BINDIR}/lua")
endif ()

if (WIN32)
    set (EXE "mechasnek.exe")
    set (PACK "packs\\liam-playground")
    file (WRITE "${CLITHER_BUILD_BINDIR}/server.bat" "${EXE} --server\r\n@PAUSE")
    install (
        FILES
            "${CLITHER_BUILD_BINDIR}/server.bat"
        DESTINATION ${CLITHER_INSTALL_BINDIR})

    if (CLITHER_BOT_API_LUA)
        file (WRITE "${CLITHER_BUILD_BINDIR}/bot.bat" "${EXE} --addr localhost --gfx --pack ${PACK} --bot lua\\bot.lua\r\n@PAUSE")
        file (WRITE "${CLITHER_BUILD_BINDIR}/bot-simple.bat" "${EXE} --addr localhost --pack ${PACK} --gfx --bot lua\\figure8.lua\r\n@PAUSE")
    install (
        FILES
            "${CLITHER_BUILD_BINDIR}/bot.bat"
            "${CLITHER_BUILD_BINDIR}/bot-simple.bat"
        DESTINATION ${CLITHER_INSTALL_BINDIR})
    endif ()
endif ()

if (EMSCRIPTEN)
    target_compile_options (clither
        PRIVATE
            # Forward-declaring FT and hb_t types causes warnings
            -Wno-typedef-redefinition
            # Containers are implemented using [1] instead of [] which causes
            # bounds warnings that are incorrect
            -Wno-array-bounds
            # Should probably write a 32-bit compatible version of morton.c
            -Wno-long-long)
    target_link_libraries (clither
        PRIVATE
            websocket.js)
    set_target_properties (clither
        PROPERTIES
            SUFFIX ".html"
            COMPILE_FLAGS "\
                -s USE_FREETYPE=1 \
                -s USE_HARFBUZZ=1 \
                -s USE_ZLIB=1"
            LINK_FLAGS "\
                -s USE_GLFW=3 \
                -s USE_FREETYPE=1 \
                -s USE_HARFBUZZ=1 \
                -s USE_ZLIB=1 \
                -s ALLOW_MEMORY_GROWTH \
                -s ASYNCIFY \
                -s EXIT_RUNTIME=1 \
                --preload-file ${CLITHER_BUILD_BINDIR}/packs@/packs \
                --bind \
                --pre-js ${PROJECT_SOURCE_DIR}/src/preamble.js \
                --shell-file ${PROJECT_SOURCE_DIR}/src/shell.html")

    file (GLOB_RECURSE PackFiles
        CONFIGURE_DEPENS "${CLITHER_BUILD_BINDIR}/packs/*")
    add_custom_command (
        OUTPUT "${PROJECT_BINARY_DIR}/.clither_emscripten_deps"
        COMMAND ${CMAKE_COMMAND} -E remove "${CLITHER_BUILD_BINDIR}/mechasnek.html"
        COMMAND ${CMAKE_COMMAND} -E touch  "${PROJECT_BINARY_DIR}/.clither_emscripten_deps"
        COMMENT "Forcing re-link of mechasnek.html"
        DEPENDS
            "src/shell.html"
            "${preamble}"
            "${PackFiles}")
    add_custom_target (clither_emscripten_deps
        DEPENDS "${PROJECT_BINARY_DIR}/.clither_emscripten_deps")
    add_dependencies (clither clither_emscripten_deps)
    install (
        FILES
            "${CLITHER_BUILD_BINDIR}/mechasnek.html"
            "${CLITHER_BUILD_BINDIR}/mechasnek.js"
            "${CLITHER_BUILD_BINDIR}/mechasnek.wasm"
            "${CLITHER_BUILD_BINDIR}/mechasnek.data"
        DESTINATION
            "${CLITHER_INSTALL_BINDIR}")
endif ()

###############################################################################
# Assets
###############################################################################

if (CLITHER_FETCH_ASSETS)
    include (ExternalProject)
    ExternalProject_Add (clither-assets
        GIT_REPOSITORY https://github.com/thecomet/clither-assets
        GIT_TAG master
        GIT_SHALLOW ON
        SOURCE_DIR ${PROJECT_BINARY_DIR}/clither-assets
        CONFIGURE_COMMAND ${CMAKE_COMMAND}
            -E copy_directory_if_different
            "${PROJECT_BINARY_DIR}/clither-assets/packs"
            "${CLITHER_BUILD_BINDIR}/packs"
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        TEST_COMMAND "")
    add_dependencies (clither clither-assets)

    # Emscripten packs the assets as part of the linker step already
    if (NOT EMSCRIPTEN)
        install (
            DIRECTORY "${CLITHER_BUILD_BINDIR}/packs/"
            DESTINATION "${CLITHER_INSTALL_BINDIR}/packs")
    endif ()
endif ()

###############################################################################
# Dependencies
###############################################################################

add_subdirectory ("thirdparty/c-ini")
c_ini_generate (settings_ini
    INPUT "include/clither/game/settings.h"
    OUTPUT_HEADER ${PROJECT_BINARY_DIR}/include/clither/game/settings_ini.h
    OUTPUT_SOURCE ${PROJECT_BINARY_DIR}/src/game/settings_ini.c)
c_ini_generate (resource_pack_ini
    INPUT "include/clither/game/resource_pack.h"
    OUTPUT_HEADER ${PROJECT_BINARY_DIR}/include/clither/game/resource_pack_ini.h
    OUTPUT_SOURCE ${PROJECT_BINARY_DIR}/src/game/resource_pack_ini.c)
target_link_libraries (clither PRIVATE
    settings_ini
    resource_pack_ini)

if (WIN32)
    target_link_libraries (clither PRIVATE ws2_32)
    if (CLITHER_BACKTRACE)
        target_link_libraries (clither PRIVATE Dbghelp)
    endif ()
endif ()
if (UNIX)
    target_link_libraries (clither PRIVATE m)
endif ()
if (NOT EMSCRIPTEN AND NOT WIN32)
    find_package (Threads REQUIRED)
    target_link_libraries (clither PRIVATE Threads::Threads)
endif ()

if (CLITHER_AUDIO)
    find_package (FLAC)
    if (NOT FLAC_FOUND)
        set (BUILD_CXXLIBS OFF CACHE STRING "")
        set (BUILD_PROGRAMS OFF CACHE STRING "")
        set (BUILD_EXAMPLES OFF CACHE STRING "")
        set (BUILD_TESTING OFF CACHE STRING "")
        set (BUILD_DOCS OFF CACHE STRING "")
        set (INSTALL_MANPAGES OFF CACHE STRING "")
        set (INSTALL_PKGCONFIG_MODULES OFF CACHE STRING "")
        set (INSTALL_CMAKE_CONFIG_MODULE OFF CACHE STRING "")
        set (WITH_OGG OFF CACHE STRING "")
        if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            set (WITH_FORTIFY_SOURCE OFF CACHE STRING "")
        endif ()
        add_subdirectory ("thirdparty/flac-1.5.0")
        target_compile_options (FLAC PRIVATE
            $<$<C_COMPILER_ID:GNU>:-Wno-cast-function-type>
            $<$<C_COMPILER_ID:Clang>:-Wno-cast-function-type>)
        set_target_properties (FLAC PROPERTIES
            MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
    endif ()
    target_link_libraries (clither PRIVATE FLAC::FLAC)

    add_subdirectory ("thirdparty/speex-1.2.1")
    set_target_properties (speex PROPERTIES
        MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
    target_link_libraries (clither PRIVATE speex)

    if (CLITHER_AUDIO_OPENAL AND NOT EMSCRIPTEN)
        find_package (OpenAL)
        if (NOT OpenAL_FOUND)
            set (ALSOFT_DLOPEN OFF CACHE STRING "")
            set (ALSOFT_UTILS OFF CACHE STRING "")
            set (ALSOFT_EXAMPLES OFF CACHE STRING "")
            set (ALSOFT_INSTALL OFF CACHE STRING "")
            set (ALSOFT_INSTALL_CONFIG OFF CACHE STRING "")
            set (ALSOFT_INSTALL_HRTF_DATA OFF CACHE STRING "")
            set (ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE STRING "")
            set (ALSOFT_INSTALL_EXAMPLES OFF CACHE STRING "")
            set (ALSOFT_INSTALL_UTILS OFF CACHE STRING "")
            add_subdirectory ("thirdparty/openal-soft-1.24.3")
            target_compile_options (OpenAL PRIVATE
                $<$<C_COMPILER_ID:GNU>:-Wno-inline -Wno-ignored-attributes>
                $<$<C_COMPILER_ID:Clang>:-Wno-inline -Wno-ignored-attributes>)
            set_target_properties (OpenAL PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY ${CLITHER_BUILD_BINDIR}
                MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
        endif ()
        target_link_libraries (clither PRIVATE OpenAL::OpenAL)
    endif ()
endif ()

# Emscripten has its own freetype + harfbuzz
if (CLITHER_GFX AND NOT EMSCRIPTEN)
    find_package (Freetype)
    if (NOT FREETYPE_FOUND)
        set (FT_DISABLE_HARFBUZZ ON CACHE STRING "")
        set (SKIP_INSTALL_ALL ON CACHE STRING "")
        add_subdirectory ("thirdparty/freetype-2.13.3")
        set_target_properties (freetype PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
        add_library (Freetype::Freetype ALIAS freetype)
    endif ()
    target_link_libraries (clither PRIVATE Freetype::Freetype)

    find_package (HarfBuzz)
    if (NOT HarfBuzz_FOUND)
        set (HB_BUILD_SUBSET ON CACHE STRING "")
        set (HB_HAVE_FREETYPE ON CACHE STRING "")
        set (SKIP_INSTALL_ALL ON CACHE STRING "")
        add_subdirectory ("thirdparty/harfbuzz-11.0.0")
        set_target_properties (harfbuzz PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
        if (WIN32)
            target_compile_options (harfbuzz PRIVATE
                $<$<C_COMPILER_ID:GNU>:-Wa,-mbig-obj>
                $<$<C_COMPILER_ID:Clang>:-Wa,-mbig-obj>
                $<$<C_COMPILER_ID:MSVC>:/bigobj>)
            target_compile_options (harfbuzz-subset PRIVATE
                $<$<C_COMPILER_ID:GNU>:-Wa,-mbig-obj>
                $<$<C_COMPILER_ID:Clang>:-Wa,-mbig-obj>
                $<$<C_COMPILER_ID:MSVC>:/bigobj>)
        endif ()
        add_library (HarfBuzz::HarfBuzz ALIAS harfbuzz)
    endif ()
    target_link_libraries (clither PRIVATE HarfBuzz::HarfBuzz)
endif ()

if (CLITHER_GFX_SDL)
    set (SDL2_DISABLE_INSTALL ON CACHE STRING "")
    set (SDL2_DISABLE_SDL2MAIN ON CACHE STRING "")
    set (SDL2_DISABLE_UNINSTALL ON CACHE STRING "")
    set (SDL_SHARED OFF CACHE STRING "")
    set (SDL_STATIC ON CACHE STRING "")
    set (SDL_TEST OFF CACHE STRING "")
    add_subdirectory ("thirdparty/SDL-2.26.5")
    set_target_properties (SDL2-static PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
    target_link_libraries (clither PRIVATE SDL2-static)

    set (BUILD_SHARED_LIBS OFF CACHE STRING "")
    set (SDL2IMAGE_INSTALL OFF CACHE STRING "")
    set (SDL2IMAGE_SAMPLES OFF CACHE STRING "")
    add_subdirectory ("thirdparty/SDL_image-2.6.3")
    set_target_properties (SDL2_image PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
    target_link_libraries (clither PRIVATE SDL2_image)
endif ()

# Emscripten ships with GLFW already
if (CLITHER_GFX_GLES2 AND NOT EMSCRIPTEN)
    find_package (GLFW3)
    if (NOT GLFW3_FOUND)
        set (GLFW_BUILD_EXAMPLES OFF CACHE STRING "")
        set (GLFW_BUILD_TESTS OFF CACHE STRING "")
        set (GLFW_BUILD_DOCS OFF CACHE STRING "")
        set (GLFW_INSTALL OFF CACHE STRING "")
        add_subdirectory ("thirdparty/glfw-3.3.8")
        set_target_properties (glfw PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
        add_library (GLFW3::GLFW3 ALIAS glfw)
    endif ()
    target_link_libraries (clither PRIVATE GLFW3::GLFW3)
endif ()

if (CLITHER_GFX_GLES2)
    target_include_directories (clither
        PRIVATE
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/thirdparty/stb>)
endif ()

if (CLITHER_BOT_API_LUA)
    add_subdirectory ("thirdparty/lua-5.4.7")
    set_target_properties (lua-lib PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
    target_link_libraries (clither PRIVATE lua-lib)
endif ()

if (CLITHER_TESTS)
    enable_language (CXX)
    add_subdirectory ("thirdparty/googletest")
    set_target_properties (gmock PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
    target_link_libraries (clither PRIVATE gmock)
endif ()

if (CLITHER_BENCHMARKS)
    enable_language (CXX)
    add_subdirectory ("thirdparty/benchmark")
    set_target_properties (benchmark PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
    target_link_libraries (clither PRIVATE benchmark)
endif ()

if (CLITHER_DOC)
    add_subdirectory ("doc/clither-doc")
endif ()
