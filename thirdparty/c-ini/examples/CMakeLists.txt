project ("c-ini-examples"
    LANGUAGES C)

# Example 1
c_ini_generate (example1_parser
    INPUT "example1.c"
    OUTPUT_HEADER "${PROJECT_BINARY_DIR}/example1/include/example1/settings_ini.h"
    OUTPUT_SOURCE "${PROJECT_BINARY_DIR}/example1/src/settings_ini.c")
add_executable (c_ini_example1 "example1.c" )
target_link_libraries (c_ini_example1 PUBLIC example1_parser)
target_include_directories (c_ini_example1 PRIVATE
    "${PROJECT_BINARY_DIR}/example1/include")
target_compile_options (c_ini_example1 PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -pedantic -ansi>)
set_target_properties (c_ini_example1 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Example 2
c_ini_generate (example2_parser
    INPUT "example2_client.h" "example2_server.h"
    OUTPUT_HEADER "${PROJECT_BINARY_DIR}/example2/include/example2/settings_ini.h"
    OUTPUT_SOURCE "${PROJECT_BINARY_DIR}/example2/src/settings_ini.c")
add_executable (c_ini_example2 "example2.c")
target_link_libraries (c_ini_example2 PRIVATE example2_parser)
target_include_directories (c_ini_example2 PRIVATE
    "${PROJECT_BINARY_DIR}/example2/include")
target_compile_options (c_ini_example2 PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -pedantic -ansi>)
set_target_properties (c_ini_example2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Example 3
c_ini_generate (example3_parser
    INPUT "example3.c"
    OUTPUT_HEADER "${PROJECT_BINARY_DIR}/example3/include/example3/player_ini.h"
    OUTPUT_SOURCE "${PROJECT_BINARY_DIR}/example3/src/player_ini.c")
add_executable (c_ini_example3 "example3.c")
target_link_libraries (c_ini_example3 PRIVATE example3_parser)
target_include_directories (c_ini_example3 PRIVATE
    "${PROJECT_BINARY_DIR}/example3/include")
target_compile_options (c_ini_example3 PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -pedantic -ansi>)
set_target_properties (c_ini_example3 PROPERTIES
    C_STANDARD 99
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
