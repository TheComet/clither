project ("C Structures"
    VERSION 0.0.1
    LANGUAGES C)

###############################################################################
# Options and config
###############################################################################

if (CMAKE_BUILD_TYPE MATCHES Debug)
    set (DEBUG_FEATURE ON)
endif ()
if (CMAKE_PLATFORM_ID MATCHES Windows AND DEBUG_FEATURE)
    set (WINDOWS_AND_WINDOWS_FEATURE ON)
endif ()

set (CSTRUCTURES_LIB_TYPE "STATIC" CACHE STRING "Build as shared or static")
set (CSTRUCTURES_BTREE_EXPAND_FACTOR "2" CACHE STRING "When reallocating btree memory, this is the factor with which the buffer grows")
set (CSTRUCTURES_BTREE_MIN_CAPACITY "32" CACHE STRING "The smallest number of elements to reserve when initializing a btree")
set (CSTRUCTURES_VEC_EXPAND_FACTOR "2" CACHE STRING "When reallocating vector memory, this is the factor with which the buffer grows")
set (CSTRUCTURES_VEC_MIN_CAPACITY "32" CACHE STRING "The smallest number of elements to reserve when initializing a vector")
option (CSTRUCTURES_BTREE_64BIT_KEYS "Enable 64-bit keys for btrees instead of 32-bit keys" OFF)
option (CSTRUCTURES_BTREE_64BIT_CAPACITY "Enable btrees to allow up to 2^64 entries instead of 2^32" OFF)
option (CSTRUCTURES_MEMORY_BACKTRACE "Enable generating backtraces to every malloc/realloc call, making it easy to find where memory leaks occur" ${DEBUG_WINDOWS_AND_WINDOWS_FEATURE})
option (CSTRUCTURES_MEMORY_DEBUGGING "Enable tracking malloc/realloc/free calls to detect memory leaks" ${DEBUG_FEATURE})
option (CSTRUCTURES_PIC "Generate position independent code" OFF)
option (CSTRUCTURES_PROFILING "Enable -pg and -fno-omit-frame-pointer" OFF)
option (CSTRUCTURES_VEC_64BIT "Set vector capacity to 2^64 instead of 2^32, but makes the structure 32 bytes instead of 20 bytes" OFF)

configure_file ("templates/config.h.in" "include/cstructures/config.h")

###############################################################################
# Library source files and settings
###############################################################################

add_library (cstructures ${CSTRUCTURES_LIB_TYPE}
    "src/btree.c"
    "src/hash.c"
    "src/hashmap.c"
    "src/init.c"
    "src/memory.c"
    "src/string.c"
    "src/vector.c"

    "include/cstructures/backtrace.h"
    "include/cstructures/btree.h"
    "include/cstructures/hash.h"
    "include/cstructures/hashmap.h"
    "include/cstructures/init.h"
    "include/cstructures/memory.h"
    "include/cstructures/string.h"
    "include/cstructures/vector.h"

    $<$<PLATFORM_ID:Linux>:src/platform/linux/backtrace_linux.c>
    $<$<PLATFORM_ID:Windows>:src/platform/win32/backtrace_win32.c>)
target_include_directories (cstructures
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_compile_options (cstructures
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/EHa /MTd /W4 /wd4305 /wd4201 /wd4706 /wd4100 /wd4244 /wd4477 /wd4003 /D_CRT_SECURE_NO_DEPRECATE>
        $<$<C_COMPILER_ID:GNU>:-W -Wall -Wextra -Wshadow -Wconversion -Wno-unused-parameter $<$<BOOL:${CSTRUCTURES_PROFILING}>:-pg -fno-omit-frame-pointer>>
        $<$<C_COMPILER_ID:Clang>:-W -Wall -Wextra -Wshadow -Wconversion -Wno-unused-parameter $<$<BOOL:${CSTRUCTURES_PROFILING}>:-pg -fno-omit-frame-pointer>>)
set_target_properties (cstructures
    PROPERTIES
        POSITION_INDEPENDENT_CODE ${CSTRUCTURES_PIC})

###############################################################################
# Unit tests
###############################################################################

if (CSTRUCTURES_TESTS)
    enable_language (CXX)
    add_executable (cstructures_tests
        "src/tests/test_btree.cpp"
        "src/tests/test_btree_as_set.cpp"
        "src/tests/test_hashmap.cpp"
        "src/tests/test_vector.cpp"
        "src/tests/env_library_init.cpp"
        "src/tests/main.cpp")
    target_link_libraries (cstructures_tests PRIVATE cstructures)
    target_link_libraries (cstructures_tests PRIVATE gmock)
endif ()

###############################################################################
# Benchmarks
###############################################################################

if (CSTRUCTURES_BENCHMARKS)
    enable_language (CXX)
    add_executable (cstructures_benchmarks
        "src/benchmarks/bench_hashmap.cpp"
        "src/benchmarks/bench_std_unordered_map.cpp"
        "src/benchmarks/bench_vector.cpp"
        "src/benchmarks/bench_std_vector.cpp"
        "src/benchmarks/main.cpp")
    target_link_libraries (cstructures_benchmarks PRIVATE cstructures)
    target_link_libraries (cstructures_benchmarks PRIVATE benchmark)
    target_include_directories (cstructures_benchmarks
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/thirdparty/benchmark/include>)
endif ()

