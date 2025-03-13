#pragma once

#include "clither/config.h"

#if defined(CLITHER_BACKTRACE)
int backtrace_init(void);
void backtrace_deinit(void);

/*!
 * @brief Generates a backtrace.
 * @param[in] size The maximum number of frames to walk.
 * @return Returns an array of char* arrays.
 * @note The returned array must be freed manually with FREE(returned_array).
 */
char** backtrace_get(int* size);
void backtrace_free(char** bt);
#else
#    define backtrace_init() (0)
#    define backtrace_deinit()
#    define backtrace_get(x) (NULL)
#    define backtrace_free(x)
#endif
