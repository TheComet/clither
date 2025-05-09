cmake_minimum_required (VERSION 3.21)

project ("c-ini"
    VERSION 1.0.0
    LANGUAGES C)

add_executable (c-ini
    "src/c-ini.c")
target_compile_options (c-ini
    PRIVATE
        $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -pedantic -ansi>)
set_target_properties (c-ini
    PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)
