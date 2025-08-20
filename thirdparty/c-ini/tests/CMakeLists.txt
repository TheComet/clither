enable_testing ()

project ("c-ini-tests"
    LANGUAGES C CXX)

# GoogleTest requires at least C++17
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

include (FetchContent)
FetchContent_Declare (
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.x)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set (gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

c_ini_generate (test_parse_types
    INPUT "test_parse_types.cpp"
    OUTPUT_HEADER "${PROJECT_BINARY_DIR}/test_parse_types.h"
    OUTPUT_SOURCE "${PROJECT_BINARY_DIR}/test_parse_types.c")
c_ini_generate (test_defaults
    INPUT "test_defaults.cpp"
    OUTPUT_HEADER "${PROJECT_BINARY_DIR}/test_defaults.h"
    OUTPUT_SOURCE "${PROJECT_BINARY_DIR}/test_defaults.c")
c_ini_generate (test_fixed_str
    INPUT "test_fixed_str.cpp"
    OUTPUT_HEADER "${PROJECT_BINARY_DIR}/test_fixed_str.h"
    OUTPUT_SOURCE "${PROJECT_BINARY_DIR}/test_fixed_str.c")
c_ini_generate (test_dynamic_str
    INPUT "test_dynamic_str.cpp"
    OUTPUT_HEADER "${PROJECT_BINARY_DIR}/test_dynamic_str.h"
    OUTPUT_SOURCE "${PROJECT_BINARY_DIR}/test_dynamic_str.c")
c_ini_generate (test_custom_str
    INPUT "test_custom_str.cpp"
    OUTPUT_HEADER "${PROJECT_BINARY_DIR}/test_custom_str.h"
    OUTPUT_SOURCE "${PROJECT_BINARY_DIR}/test_custom_str.c"
    INCLUDE_FILES "custom_str.h")
c_ini_generate (test_fixed_strlist
    INPUT "test_fixed_strlist.cpp"
    OUTPUT_HEADER "${PROJECT_BINARY_DIR}/test_fixed_strlist.h"
    OUTPUT_SOURCE "${PROJECT_BINARY_DIR}/test_fixed_strlist.c")
c_ini_generate (test_dynamic_strlist
    INPUT "test_dynamic_strlist.cpp"
    OUTPUT_HEADER "${PROJECT_BINARY_DIR}/test_dynamic_strlist.h"
    OUTPUT_SOURCE "${PROJECT_BINARY_DIR}/test_dynamic_strlist.c")
c_ini_generate (test_custom_strlist
    INPUT "test_custom_strlist.cpp"
    OUTPUT_HEADER "${PROJECT_BINARY_DIR}/test_custom_strlist.h"
    OUTPUT_SOURCE "${PROJECT_BINARY_DIR}/test_custom_strlist.c"
    INCLUDE_FILES "custom_strlist.h")

add_executable (c_ini_tests
    "test_parse_types.cpp"
    "test_defaults.cpp"
    "test_fixed_str.cpp"
    "test_dynamic_str.cpp"
    "test_custom_str.cpp"
    "custom_str.cpp"
    "test_fixed_strlist.cpp"
    "test_dynamic_strlist.cpp"
    "test_custom_strlist.cpp"
    "custom_strlist.cpp")
target_include_directories (c_ini_tests PRIVATE
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}")
target_link_libraries (c_ini_tests PRIVATE
    GTest::gmock
    GTest::gmock_main
    test_parse_types
    test_defaults
    test_fixed_str
    test_dynamic_str
    test_custom_str
    test_fixed_strlist
    test_dynamic_strlist
    test_custom_strlist)
set_target_properties (c_ini_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
