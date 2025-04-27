cmake_minimum_required (VERSION 3.24)

option (LUA_INTERPRETER "Build the Lua interpreter" OFF)
option (LUA_COMPILER "Build the Lua compiler" OFF)

project ("lua"
    LANGUAGES C
    VERSION 5.4.7)

add_subdirectory ("src")
