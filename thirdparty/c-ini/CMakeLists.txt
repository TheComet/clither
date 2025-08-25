cmake_minimum_required (VERSION 3.21)

project ("c-ini"
    VERSION 1.0.0
    LANGUAGES C)

option (C_INI_EXAMPLES "Build examples" OFF)
option (C_INI_TESTS "Build tests" OFF)

if (CMAKE_CROSSCOMPILING AND NOT NATIVE_C_COMPILER)
    find_program (NATIVE_C_COMPILER NAMES gcc clang cl cl.exe)
    if (NOT NATIVE_C_COMPILER)
        message (FATAL_ERROR "Could not find a native C compiler. Use -DNATIVE_C_COMPILER=/path/to/cc to to set the native compiler manually")
    endif ()
    message (STATUS "Using native compiler: ${NATIVE_C_COMPILER}\nYou may want to use -DNATIVE_C_COMPILER=/path/to/cc to to set the native compiler manually")
endif ()

if (NOT CMAKE_CROSSCOMPILING)
    add_executable (c_ini_generator "c-ini/c-ini.c")
    target_compile_options (c_ini_generator PRIVATE
        $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -pedantic -ansi>)
    set_target_properties (c_ini_generator PROPERTIES
        C_STANDARD 90
        C_EXTENSIONS NO)
else ()
    include (ExternalProject)
    ExternalProject_Add (c_ini_generator_build
        PREFIX ${PROJECT_BINARY_DIR}/build-host
        SOURCE_DIR ${PROJECT_SOURCE_DIR}
        BINARY_DIR ${PROJECT_BINARY_DIR}/build-host
        CMAKE_ARGS -DCMAKE_C_COMPILER=${NATIVE_C_COMPILER}
        INSTALL_COMMAND "")
    add_executable (c_ini_generator IMPORTED GLOBAL)
    add_dependencies(c_ini_generator c_ini_generator_build)
    set_property (TARGET c_ini_generator PROPERTY
        IMPORTED_LOCATION ${PROJECT_BINARY_DIR}/build-host/c_ini_generator)

    # THIS STRAIGHT UP DOESN'T WORK
    #add_custom_command (
    #    COMMAND ${CMAKE_COMMAND}
    #        -S ${CMAKE_CURRENT_SOURCE_DIR}
    #        -B ${CMAKE_CURRENT_BINARY_DIR}/build-host
    #    COMMAND ${CMAKE_COMMAND}
    #        --build ${CMAKE_CURRENT_BINARY_DIR}/build-host
    #    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/build-host/c_ini_generator
    #    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/c-ini/c-ini.c
    #    COMMENT "Building c_ini_generator with native compiler")

    # THIS IS UGLY
    #execute_process (
    #    COMMAND ${CMAKE_COMMAND}
    #        -S ${CMAKE_CURRENT_SOURCE_DIR}
    #        -B ${CMAKE_CURRENT_BINARY_DIR}/build-host)
    #execute_process (
    #    COMMAND ${CMAKE_COMMAND}
    #        --build ${CMAKE_CURRENT_BINARY_DIR}/build-host)
    #add_executable (c_ini_generator IMPORTED GLOBAL)
    #set_property (TARGET c_ini_generator PROPERTY
    #    IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/build-host/c_ini_generator)
endif ()

set (C_INI_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/c-ini" CACHE STRING "")

function (c_ini_generate target)
    cmake_parse_arguments (ARG
        ""
        "OUTPUT_HEADER;OUTPUT_SOURCE"
        "INCLUDE_FILES;INPUT"
        ${ARGN})
    if (NOT ARG_OUTPUT_HEADER)
        message (FATAL_ERROR "OUTPUT_HEADER argument is required")
    endif ()
    if (NOT ARG_OUTPUT_SOURCE)
        message (FATAL_ERROR "OUTPUT_SOURCE argument is required")
    endif ()
    if (ARG_UNPARSED_ARGUMENTS)
        message (FATAL_ERROR "Unrecognized arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif ()

    get_filename_component (OUTPUT_HEADER_DIR ${ARG_OUTPUT_HEADER} DIRECTORY)
    get_filename_component (OUTPUT_SOURCE_DIR ${ARG_OUTPUT_SOURCE} DIRECTORY)

    set (ABSOLUTE_INPUT_FILES)
    foreach (INPUT_FILE IN LISTS ARG_INPUT)
        get_filename_component (ABSOLUTE_INPUT_FILE ${INPUT_FILE} ABSOLUTE)
        list (APPEND ABSOLUTE_INPUT_FILES ${ABSOLUTE_INPUT_FILE})
    endforeach ()

    if (ARG_INCLUDE_FILES)
        set (INCLUDE_FILES_ARG --include-files)
    endif ()
    
    add_custom_command (
        OUTPUT ${ARG_OUTPUT_HEADER} ${ARG_OUTPUT_SOURCE}
        COMMAND ${CMAKE_COMMAND}
            -E make_directory ${OUTPUT_HEADER_DIR} ${OUTPUT_SOURCE_DIR}
        COMMAND c_ini_generator
            --input ${ABSOLUTE_INPUT_FILES}
            ${INCLUDE_FILES_ARG} ${ARG_INCLUDE_FILES}
            --output-header ${ARG_OUTPUT_HEADER}
            --output-source ${ARG_OUTPUT_SOURCE}
        DEPENDS c_ini_generator ${ABSOLUTE_INPUT_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating C-INI source files from ${ARG_INPUT}"
        #COMMENT "--input ${ABSOLUTE_INPUT_FILES} ${INCLUDE_FILES_ARG} ${ARG_INCLUDE_FILES} --output-header ${ARG_OUTPUT_HEADER} --output-source ${ARG_OUTPUT_SOURCE}"
        VERBATIM)
    add_library (${target} INTERFACE
        ${ARG_OUTPUT_HEADER}
        ${ARG_OUTPUT_SOURCE})
    target_sources (${target} INTERFACE
        ${ARG_OUTPUT_HEADER}
        ${ARG_OUTPUT_SOURCE})
    target_include_directories (${target} INTERFACE
        ${C_INI_INCLUDE_DIR})
endfunction ()

if (C_INI_EXAMPLES)
    add_subdirectory ("examples")
endif ()
if (C_INI_TESTS)
    add_subdirectory ("tests")
endif ()
