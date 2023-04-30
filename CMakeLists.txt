cmake_minimum_required (VERSION 3.24)

if (NOT DEFINED CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif()
message (STATUS "Build type: ${CMAKE_BUILD_TYPE}")

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

project ("clither"
    VERSION 1.0.0
    LANGUAGES C)

set (CLITHER_BUILD_BINDIR "${PROJECT_BINARY_DIR}")
set (CLITHER_BUILD_LIBDIR "${CLITHER_BUILD_BINDIR}/lib")

set (CLITHER_INSTALL_BINDIR ".")
set (CLITHER_INSTALL_LIBDIR "lib")

add_executable (clither
    "src/main.c")
target_compile_options (clither
    PRIVATE
        $<$<C_COMPILER_ID:GNU>:-W -Wall -Wextra -pedantic>
        $<$<C_COMPILER_ID:Clang>:-W -Wall -Wextra -pedantic>)
set_target_properties (clither
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY ${CLITHER_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY ${CLITHER_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CLITHER_BUILD_BINDIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CLITHER_BUILD_BINDIR}
        INSTALL_RPATH ${CLITHER_INSTALL_LIBDIR})

find_package (SDL2 REQUIRED)
find_package (SDL2_gfx REQUIRED)
target_link_libraries (clither
    PRIVATE
        SDL2::Core
        SDL2::GFX)

install (
    TARGETS clither
    RUNTIME DESTINATION ${CLITHER_INSTALL_BINDIR})

