#pragma once

#define CSTRUCTURES_BACKTRACE_SIZE 64

#include "cstructures/config.h"

C_BEGIN

CSTRUCTURES_PRIVATE_API int
backtrace_init(void);

CSTRUCTURES_PRIVATE_API void
backtrace_deinit(void);

/*!
 * @brief Generates a backtrace.
 * @param[in] size The maximum number of frames to walk.
 * @return Returns an array of char* arrays.
 * @note The returned array must be freed manually with FREE(returned_array).
 */
CSTRUCTURES_PRIVATE_API char**
backtrace_get(int* size);

CSTRUCTURES_PRIVATE_API void
backtrace_free(char** bt);

C_END
