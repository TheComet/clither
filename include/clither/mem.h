#pragma once

#include "clither/config.h"
#include <stdint.h>

#if !defined(CLITHER_MEMORY_DEBUGGING)
#   include <stdlib.h>
#   define mem_init_threadlocal()    
#   define mem_deinit_threadlocal()  (0)
#   define mem_alloc                 malloc
#   define mem_free                  free
#   define mem_realloc               realloc
#   define mem_track_allocation(p)   do {} while (0)
#   define mem_track_deallocation(p) do {} while (0)
#   define mem_own(p, s) do {} while (0)
#   define mem_unown(p) do {} while  (0)
#else

/*!
 * @brief Initializes memory tracking. This is called by odbutil_init().
 *
 * In release mode this does nothing. In debug mode it will initialize
 * memory reports and backtraces, if enabled.
 */
ODBUTIL_PUBLIC_API int
init_mem_init_threadlocal(void);

/*!
 * @brief De-initializes memory tracking. This is called from odbutil_deinit().
 *
 * In release mode this does nothing. In debug mode this will output the memory
 * report and print backtraces, if enabled.
 * @return Returns the number of memory leaks.
 */
ODBUTIL_PUBLIC_API int
mem_deinit_threadlocal(void);

/*!
 * @brief Does the same thing as a normal call to malloc(), but does some
 * additional work to monitor and track down memory leaks.
 */
ODBUTIL_PUBLIC_API void*
mem_alloc(int size);

/*!
 * @brief Does the same thing as a normal call to realloc(), but does some
 * additional work to monitor and track down memory leaks.
 */
ODBUTIL_PUBLIC_API void*
mem_realloc(void* ptr, int new_size);

/*!
 * @brief Does the same thing as a normal call to fee(), but does some
 * additional work to monitor and track down memory leaks.
 */
ODBUTIL_PUBLIC_API void
mem_free(void*);

ODBUTIL_PUBLIC_API void
mem_track_allocation(void* p);

ODBUTIL_PUBLIC_API void
mem_track_deallocation(void* p);

ODBUTIL_PUBLIC_API void
mem_own(void* p, int size);

ODBUTIL_PUBLIC_API int
mem_unown(void* p);

#endif
