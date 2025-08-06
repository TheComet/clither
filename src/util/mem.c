#include "clither/util/log.h"
#include "clither/util/mem.h"
#include "clither/util/tracker.h"
#include <stdlib.h>

/* ------------------------------------------------------------------------- */
void* mem_alloc(int size)
{
    void* p = malloc(size);
    if (size == 0)
    {
        log_warn("malloc(0) called\n");
#if defined(CLITHER_BACKTRACE)
        log_backtrace();
#endif
    }

    if (p == NULL)
    {
        log_err("malloc() failed (out of memory)\n");
#if defined(CLITHER_BACKTRACE)
        log_backtrace(); /* probably won't work but may as well*/
#endif
        return NULL;
    }

    track_mem(p, size, "");
    return p;
}

/* ------------------------------------------------------------------------- */
void* mem_realloc(void* p, int new_size)
{
    uintptr_t old_addr = (uintptr_t)p;
    p = realloc(p, new_size);

    if (new_size == 0)
    {
        log_warn("realloc(0) called\n");
#if defined(CLITHER_BACKTRACE)
        log_backtrace();
#endif
    }

    if (p == NULL)
    {
        log_err("realloc() failed (out of memory)\n");
#if defined(CLITHER_BACKTRACE)
        log_backtrace(); /* probably won't work but may as well*/
#endif
        return NULL;
    }

    if (old_addr)
        untrack_mem((void*)old_addr);
    track_mem(p, new_size, "");

    return p;
}

/* ------------------------------------------------------------------------- */
void mem_free(void* p)
{
    if (p == NULL)
    {
        log_warn("free(NULL) called\n");
#if defined(CLITHER_BACKTRACE)
        log_backtrace();
#endif
    }

    untrack_mem(p);
    free(p);
}
