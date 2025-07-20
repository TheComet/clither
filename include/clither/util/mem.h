#pragma once

#include "clither/config.h"
#include <stdint.h>

#if !defined(CLITHER_DEBUG_MEMORY)
/* clang-format off */
#   include <stdlib.h>
#   define mem_alloc                  malloc
#   define mem_free                   free
#   define mem_realloc                realloc
/* clang-format on */
#else

/*!
 * @brief Does the same thing as a normal call to malloc(), but does some
 * additional work to monitor and track down memory leaks.
 */
void* mem_alloc(int size);

/*!
 * @brief Does the same thing as a normal call to realloc(), but does some
 * additional work to monitor and track down memory leaks.
 */
void* mem_realloc(void* ptr, int new_size);

/*!
 * @brief Does the same thing as a normal call to fee(), but does some
 * additional work to monitor and track down memory leaks.
 */
void mem_free(void*);

#endif
