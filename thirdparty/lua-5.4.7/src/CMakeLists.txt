set (LUA_CORE_HEADERS
    "lapi.h"
    "lcode.h"
    "luaconf.h"
    "lctype.h"
    "ldebug.h"
    "ldo.h"
    "lfunc.h"
    "lgc.h"
    "ljumptab.h"
    "llex.h"
    "llimits.h"
    "lmem.h"
    "lobject.h"
    "lopcodes.h"
    "lopnames.h"
    "lparser.h"
    "lprefix.h"
    "lstate.h"
    "lstring.h"
    "ltable.h"
    "ltm.h"
    "lua.h"
    "lualib.h"
    "lundump.h"
    "lvm.h"
    "lzio.h")
set (LUA_CORE_SOURCES
    "lapi.c"
    "lcode.c"
    "lctype.c"
    "ldebug.c"
    "ldo.c"
    "ldump.c"
    "lfunc.c"
    "lgc.c"
    "llex.c"
    "lmem.c"
    "lobject.c"
    "lopcodes.c"
    "lparser.c"
    "lstate.c"
    "lstring.c"
    "ltable.c"
    "ltm.c"
    "lundump.c"
    "lvm.c"
    "lzio.c")
set (LUA_LIB_HEADERS
    "lauxlib.h")
set (LUA_LIB_SOURCES
    "lauxlib.c"
    "lbaselib.c"
    "lcorolib.c"
    "ldblib.c"
    "liolib.c"
    "lmathlib.c"
    "loadlib.c"
    "loslib.c"
    "lstrlib.c"
    "ltablib.c"
    "lutf8lib.c"
    "linit.c")

add_library (lua-lib STATIC
    ${LUA_CORE_HEADERS}
    ${LUA_CORE_SOURCES}
    ${LUA_LIB_HEADERS}
    ${LUA_LIB_SOURCES})
target_include_directories (lua-lib
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>)
target_compile_definitions (lua-lib
    PRIVATE
        $<$<PLATFORM_ID:Linux>:LUA_USE_LINUX)
target_link_libraries (lua-lib
    PRIVATE
        $<$<PLATFORM_ID:Linux>:m>)
set_target_properties (lua-lib PROPERTIES
    C_STANDARD 90
    OUTPUT_NAME lua)

add_executable (lua
    lua.c)
target_link_libraries (lua PRIVATE lua-lib)

add_executable (luac
    luac.c)
target_link_libraries (luac PRIVATE lua-lib)

