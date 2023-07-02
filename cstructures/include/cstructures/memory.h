#pragma once

#include "cstructures/config.h"
#include <stdint.h>

#if defined(CSTRUCTURES_MEMORY_DEBUGGING)
#   define MALLOC   cstructures_malloc
#   define FREE     cstructures_free
#   define REALLOC  cstructures_realloc
#else
#   include <stdlib.h>
#   define MALLOC   malloc
#   define FREE     free
#   define REALLOC  realloc
#endif

#define XFREE(p) do { \
        if (p) \
            FREE(p); \
    } while(0)

C_BEGIN

/*!
 * @brief Initializes memory tracking for the current thread. Must be
 * called for every thread. This is called from cs_threadlocal_init().
 *
 * In release mode this does nothing. In debug mode it will initialize
 * memory reports and backtraces, if enabled.
 */
CSTRUCTURES_PRIVATE_API int
memory_threadlocal_init(void);

/*!
 * @brief De-initializes memory tracking for the current thread. This is called
 * from cs_threadlocal_deinit().
 *
 * In release mode this does nothing. In debug mode this will output the memory
 * report and print backtraces, if enabled.
 * @return Returns the number of memory leaks.
 */
CSTRUCTURES_PRIVATE_API uintptr_t
memory_threadlocal_deinit(void);

/*!
 * @brief Does the same thing as a normal call to malloc(), but does some
 * additional work to monitor and track down memory leaks.
 */
CSTRUCTURES_PUBLIC_API void*
cstructures_malloc(uintptr_t size);

/*!
 * @brief Does the same thing as a normal call to realloc(), but does some
 * additional work to monitor and track down memory leaks.
 */
CSTRUCTURES_PUBLIC_API void*
cstructures_realloc(void* ptr, uintptr_t new_size);

/*!
 * @brief Does the same thing as a normal call to fee(), but does some
 * additional work to monitor and track down memory leaks.
 */
CSTRUCTURES_PRIVATE_API void
cstructures_free(void*);

CSTRUCTURES_PUBLIC_API uintptr_t
memory_get_num_allocs(void);

CSTRUCTURES_PUBLIC_API uintptr_t
memory_get_memory_usage(void);

CSTRUCTURES_PRIVATE_API void
mutated_string_and_hex_dump(const void* data, uintptr_t size_in_bytes);

C_END
