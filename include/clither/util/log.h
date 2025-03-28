#pragma once

#include "clither/config.h"
#include "clither/strspan.h"
#include <stdarg.h> /* va_list */

void log_set_prefix(const char* prefix);
void log_set_colors(const char* set, const char* clear);

#if defined(CLITHER_LOGGING)

void log_file_open(const char* log_file);
void log_file_close(void);

void log_net_open(const char* log_file);
void log_net_close(void);

void log_bezier_open(const char* log_file);
void log_bezier_close(void);

#endif

/* General logging functions ------------------------------------------------ */

CLITHER_PRINTF_FORMAT(1, 2) void log_raw(const char* fmt, ...);
CLITHER_PRINTF_FORMAT(1, 2) void log_info(const char* fmt, ...);
CLITHER_PRINTF_FORMAT(1, 2) void log_warn(const char* fmt, ...);
CLITHER_PRINTF_FORMAT(1, 2) int log_err(const char* fmt, ...);
CLITHER_PRINTF_FORMAT(1, 2) void log_note(const char* fmt, ...);

#if defined(CLITHER_DEBUG)
CLITHER_PRINTF_FORMAT(1, 2) void log_dbg(const char* fmt, ...);
#else
#    define log_dbg(fmt, ...)
#endif

/* Specialized logging functions -------------------------------------------- */

CLITHER_PRINTF_FORMAT(1, 2) void log_net(const char* fmt, ...);

/* Memory logging functions ------------------------------------------------- */

int log_oom(int bytes, const char* func_name);

/* Parser/File logging functions -------------------------------------------- */

void log_vflc(
    const char*    filename,
    const char*    source,
    struct strspan loc,
    const char*    fmt,
    va_list        ap);
void log_flc(
    const char*    filename,
    const char*    source,
    struct strspan loc,
    const char*    fmt,
    ...);
void log_excerpt(const char* source, struct strspan loc);
