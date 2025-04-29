#include "clither/util/log.h"
#include "clither/util/mem.h"
#include "clither/util/tracker.h"
#include <stdlib.h>

struct mem
{
    struct tracker* tracker;
    struct tracker* fd_tracker;
    unsigned        ignore_malloc : 1;
};

static CLITHER_THREADLOCAL struct mem mem;

/* ------------------------------------------------------------------------- */
int mem_init_threadlocal(void)
{
    mem.ignore_malloc = 1;
    mem.tracker = tracker_create();
    if (mem.tracker == NULL)
        goto alloc_mem_tracker_failed;
    mem.fd_tracker = tracker_create();
    if (mem.fd_tracker == NULL)
        goto alloc_fd_tracker_failed;
    mem.ignore_malloc = 0;

    return 0;
alloc_fd_tracker_failed:
    tracker_destroy(mem.tracker);
    mem.tracker = NULL;
alloc_mem_tracker_failed:
    mem.ignore_malloc = 0;
    return -1;
}

/* ------------------------------------------------------------------------- */
int mem_deinit_threadlocal(void)
{
    int leaks = 0;

    mem.ignore_malloc = 1;
    leaks += tracker_destroy(mem.tracker);
    leaks += tracker_destroy(mem.fd_tracker);
    mem.ignore_malloc = 0;

    return leaks;
}

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

    if (!mem.ignore_malloc)
    {
        mem.ignore_malloc = 1;
        tracker_track(mem.tracker, p, size);
        mem.ignore_malloc = 0;
    }

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

    if (!mem.ignore_malloc)
    {
        mem.ignore_malloc = 1;
        if (old_addr)
            tracker_untrack(mem.tracker, (void*)old_addr);
        tracker_track(mem.tracker, p, new_size);
        mem.ignore_malloc = 0;
    }

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

    if (!mem.ignore_malloc)
    {
        mem.ignore_malloc = 1;
        tracker_untrack(mem.tracker, p);
        mem.ignore_malloc = 0;
    }

    free(p);
}

/* ------------------------------------------------------------------------- */
void mem_track_allocation(void* p, int size)
{
    if (mem.ignore_malloc)
        return;
    mem.ignore_malloc = 1;
    tracker_track(mem.tracker, p, size);
    mem.ignore_malloc = 0;
}
int mem_track_deallocation(void* p)
{
    int size;
    if (mem.ignore_malloc)
        return 0;

    mem.ignore_malloc = 1;
    size = tracker_untrack(mem.tracker, p);
    mem.ignore_malloc = 0;

    return size;
}

/* ------------------------------------------------------------------------- */
void mem_track_fd(int fd)
{
    if (mem.ignore_malloc)
        return;
    mem.ignore_malloc = 1;
    tracker_track(mem.fd_tracker, (void*)(uintptr_t)fd, 0);
    mem.ignore_malloc = 0;
}
void mem_untrack_fd(int fd)
{
    if (mem.ignore_malloc)
        return;
    mem.ignore_malloc = 1;
    tracker_untrack(mem.fd_tracker, (void*)(uintptr_t)fd);
    mem.ignore_malloc = 0;
}
