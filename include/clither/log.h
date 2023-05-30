#pragma once

#include "clither/config.h"

C_BEGIN

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

/* General logging functions ----------------------------------------------- */
PRINTF_FORMAT(1, 2) void
log_dbg(const char* fmt, ...);

PRINTF_FORMAT(1, 2) void
log_info(const char* fmt, ...);

PRINTF_FORMAT(1, 2) void
log_warn(const char* fmt, ...);

PRINTF_FORMAT(1, 2) void
log_err(const char* fmt, ...);

PRINTF_FORMAT(1, 2) void
log_note(const char* fmt, ...);

/* Specialized logging functions ------------------------------------------- */

PRINTF_FORMAT(1, 2) void
log_net(const char* fmt, ...);

C_END
