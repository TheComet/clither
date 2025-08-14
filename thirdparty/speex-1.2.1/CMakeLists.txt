cmake_minimum_required (VERSION 3.20)

project (speex
    LANGUAGES C
    VERSION 1.2.1)

option (SPEEX_FLOATING_POINT "Build fixed point or floating point version" ON)
option (SPEEX_SSE "Build with SSE optimizations" ON)

file (READ "config.h.in" CONFIG_H_CONTENTS)
string (REGEX REPLACE "# *undef *([A-Za-z0-9_]+)" "#cmakedefine \\1 @\\1@" CONFIG_H_CONTENTS "${CONFIG_H_CONTENTS}")
file (WRITE "${PROJECT_BINARY_DIR}/config.h.in" "${CONFIG_H_CONTENTS}")

set (EXPORT "__attribute__((visibility(\"default\")))")
if (SPEEX_FLOATING_POINT)
    set (FLOATING_POINT 1)
else ()
    set (FIXED_POINT 1)
endif ()

set (HAVE_ALLOCA_H 1)
set (HAVE_DLFCN_H 1)
set (HAVE_INTTYPES_H 1)
set (HAVE_STDINT_H 1)
set (HAVE_STDIO_H 1)
set (HAVE_STDLIB_H 1)
set (HAVE_STRINGS_H 1)
set (HAVE_STRING_H 1)
set (HAVE_SYS_SOUNDCARD_H 1)
set (HAVE_SYS_STAT_H 1)
set (HAVE_SYS_TYPES_H 1)
set (HAVE_UNISTD_H 1)
set (LT_OBJDIR ".libs/")

set (PACKAGE_BUGREPORT "\"speex-dev@xiph.org\"")
set (PACKAGE_NAME "\"speex\"")
set (PACKAGE_STRING "\"${PACKAGE_NAME} ${PROJECT_VERSION}\"")
set (PACKAGE_TARNAME "\"speex\"")
set (PACKAGE_VERSION "\"${PROJECT_VERSION}\"")

set (SIZEOF_INT 4)
set (SIZEOF_INT16_T 2)
set (SIZEOF_INT32_T 4)
set (SIZEOF_LONG 8)
set (SIZEOF_SHORT 2)
set (SIZEOF_UINT16_T 2)
set (SIZEOF_UINT32_T 4)
set (SIZEOF_U_INT16_T 2)
set (SIZEOF_U_INT32_T 4)

set (SPEEX_MAJOR_VERSION ${PROJECT_VERSION_MAJOR})
set (SPEEX_MINOR_VERSION ${PROJECT_VERSION_MINOR})
set (SPEEX_MICRO_VERSION ${PROJECT_VERSION_PATCH})
set (SPEEX_EXTRA_VERSION "\"\"")
set (SPEEX_VERSION "\"${PROJECT_VERSION}\"")

if (SPEEX_SSE AND NOT EMSCRIPTEN)
    set (_USE_SSE 1)
endif ()

if (WIN32)
    set (HAVE_SYS_SOUNDCARD_H 0)
    set (HAVE_UNISTD_H 0)
    set (HAVE_DLFCN_H 0)
    set (HAVE_ALLOCA_H 0)
    set (HAVE_SYS_STAT_H 0)
    set (HAVE_SYS_TYPES_H 0)
endif ()

set (inline "__inline__")
set (restrict "__restrict__")

configure_file ("${PROJECT_BINARY_DIR}/config.h.in" "config.h")

set (SIZE16 "int16_t")
set (USIZE16 "uint16_t")
set (SIZE32 "int32_t")
set (USIZE32 "uint32_t")
set (INCLUDE_STDINT "#include <stdint.h>")
configure_file ("include/speex/speex_config_types.h.in" "speex_config_types.h")

add_library (speex STATIC
    "libspeex/bits.c"
    "libspeex/cb_search.c"
    "libspeex/exc_10_16_table.c"
    "libspeex/exc_10_32_table.c"
    "libspeex/exc_20_32_table.c"
    "libspeex/exc_5_256_table.c"
    "libspeex/exc_5_64_table.c"
    "libspeex/exc_8_128_table.c"
    "libspeex/filters.c"
    "libspeex/gain_table.c"
    "libspeex/gain_table_lbr.c"
    "libspeex/hexc_10_32_table.c"
    "libspeex/hexc_table.c"
    "libspeex/high_lsp_tables.c"
    "libspeex/kiss_fft.c"
    "libspeex/kiss_fftr.c"
    "libspeex/lpc.c"
    "libspeex/lsp.c"
    "libspeex/lsp_tables_nb.c"
    "libspeex/ltp.c"
    "libspeex/modes.c"
    "libspeex/modes_wb.c"
    "libspeex/nb_celp.c"
    "libspeex/quant_lsp.c"
    "libspeex/sb_celp.c"
    "libspeex/smallft.c"
    "libspeex/speex.c"
    "libspeex/speex_callbacks.c"
    "libspeex/speex_header.c"
    "libspeex/stereo.c"
    "libspeex/vbr.c"
    "libspeex/vorbis_psy.c"
    "libspeex/vq.c"
    "libspeex/window.c")
target_include_directories (speex
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_BINARY_DIR}") # config.h
target_compile_definitions (speex
    PUBLIC
        HAVE_CONFIG_H)
target_compile_options (speex
    PRIVATE
        $<$<C_COMPILER_ID:GNU>:-ffast-math>
        $<$<C_COMPILER_ID:Clang>:-ffast-math>)
