#pragma once

#include "clither/config.h"

void
log_set_prefix(const char* prefix);
void
log_set_colors(const char* set, const char* clear);

#if defined(CLITHER_LOGGING)
void
log_file_open(const char* log_file);
void
log_file_close();
#endif

__attribute__ ((format(printf, 1, 2)))
void log_info(const char* fmt, ...);

__attribute__ ((format(printf, 1, 2)))
void log_warn(const char* fmt, ...);

__attribute__ ((format(printf, 1, 2)))
void log_err(const char* fmt, ...);

__attribute__ ((format(printf, 1, 2)))
void log_dbg(const char* fmt, ...);
