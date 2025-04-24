#include "clither/platform/backtrace.h"
#include "clither/util/cli_colors.h"
#include "clither/util/hash.h"
#include "clither/util/hmap.h"
#include "clither/util/log.h"
#include "clither/util/mem.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct report_info
{
    uintptr_t location;
    int       size;
#if defined(CLITHER_BACKTRACE)
    int    backtrace_size;
    char** backtrace;
#endif
};

HMAP_DECLARE_HASH(
    static, report_hmap, hash32, uintptr_t, struct report_info, 32)
HMAP_DEFINE_HASH(
    static,
    report_hmap,
    hash32,
    uintptr_t,
    struct report_info,
    32,
    hash32_aligned_ptr)

struct state
{
    struct report_hmap* report;
    int                 allocations;
    int                 deallocations;
    unsigned            ignore_malloc : 1;
};

static CLITHER_THREADLOCAL struct state state;

void mem_init_threadlocal(void)
{
    state.allocations = 0;
    state.deallocations = 0;

    report_hmap_init(&state.report);
}

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_BACKTRACE)
static void print_backtrace(void)
{
    char** bt;
    int    bt_size, i;

    if (state.ignore_malloc)
        return;

    if (!(bt = backtrace_get(&bt_size)))
    {
        fprintf(stderr, "Failed to generate backtrace\n");
        return;
    }

    for (i = BACKTRACE_OMIT_COUNT; i < bt_size; ++i)
    {
        if (strstr(bt[i], "invoke_main"))
            break;
        fprintf(stderr, "  %s\n", bt[i]);
    }
    backtrace_free(bt);
}
#else
#    define print_backtrace()
#endif

/* ------------------------------------------------------------------------- */
static void track_allocation(uintptr_t addr, int size)
{
    struct report_info* info;
    ++state.allocations;

    if (size == 0)
    {
        fprintf(stderr, "malloc(0)\n");
#if defined(CLITHER_BACKTRACE)
        print_backtrace();
#endif
        return;
    }

    if (state.ignore_malloc)
        return;

    /* insert info into hashmap */
    state.ignore_malloc = 1;
    info = report_hmap_emplace_new(&state.report, addr);
    state.ignore_malloc = 0;
    if (info == NULL)
    {
        fprintf(
            stderr,
            "Double allocation! This is usually caused by calling "
            "mem_track_allocation() on the same address twice.\n");
        print_backtrace();
        return;
    }

    /* record the location and size of the allocation */
    info->location = addr;
    info->size = size;

    /* Create backtrace to this allocation */
#if defined(CLITHER_BACKTRACE)
    state.ignore_malloc = 1;
    if (!(info->backtrace = backtrace_get(&info->backtrace_size)))
        fprintf(stderr, "Failed to generate backtrace\n");
    state.ignore_malloc = 0;
#endif
}

static void track_deallocation(uintptr_t addr, const char* free_type)
{
    struct report_info* info;
    state.deallocations++;

    if (addr == 0)
    {
        fprintf(stderr, "free(NULL)\n");
#if defined(CLITHER_BACKTRACE)
        print_backtrace();
#endif
        return;
    }

    if (state.ignore_malloc)
        return;

    /* find matching allocation and remove from hashmap */
    info = report_hmap_erase(state.report, addr);
    if (info)
    {
#if defined(CLITHER_BACKTRACE)
        if (info->backtrace)
            backtrace_free(info->backtrace);
        else
            fprintf(
                stderr, "Allocation didn't have a backtrace (it was NULL)\n");
#endif
    }
    else
    {
        fprintf(
            stderr, "%s'ing something that was never allocated\n", free_type);
#if defined(CLITHER_BACKTRACE)
        print_backtrace();
#endif
    }
}

static void acquire(uintptr_t addr, int size)
{
    struct report_info* info;

    if (addr == 0)
        return;

    ++state.allocations;

    /* insert info into hashmap */
    state.ignore_malloc = 1;
    info = report_hmap_emplace_new(&state.report, addr);
    state.ignore_malloc = 0;
    if (info == NULL)
    {
        fprintf(
            stderr,
            "Double allocation! This is usually caused by calling "
            "mem_own() on the same address twice.\n");
        print_backtrace();
        return;
    }

    /* record the location and size of the allocation */
    info->location = addr;
    info->size = size;

    /* Create backtrace to this allocation */
#if defined(CLITHER_BACKTRACE)
    state.ignore_malloc = 1;
    if (!(info->backtrace = backtrace_get(&info->backtrace_size)))
        fprintf(stderr, "Failed to generate backtrace\n");
    state.ignore_malloc = 0;
#endif
}

static int release(uintptr_t addr)
{
    struct report_info* info;

    if (addr == 0)
        return 0;

    state.deallocations++;

    /* find matching allocation and remove from hashmap */
    info = report_hmap_erase(state.report, addr);
    if (info)
    {
#if defined(CLITHER_BACKTRACE)
        if (info->backtrace)
            backtrace_free(info->backtrace);
        else
            fprintf(
                stderr, "Allocation didn't have a backtrace (it was NULL)\n");
#endif
        return info->size;
    }

    fprintf(stderr, "releasing something that was never allocated\n");
#if defined(CLITHER_BACKTRACE)
    print_backtrace();
#endif
    return 0;
}

/* ------------------------------------------------------------------------- */
void* mem_alloc(int size)
{
    void* p = malloc(size);
    if (p == NULL)
    {
        fprintf(stderr, "malloc() failed (out of memory)\n");
#if defined(CLITHER_BACKTRACE)
        print_backtrace(); /* probably won't work but may as well*/
#endif
        return NULL;
    }

    track_allocation((uintptr_t)p, size);
    return p;
}

/* ------------------------------------------------------------------------- */
void* mem_realloc(void* p, int new_size)
{
    uintptr_t old_addr = (uintptr_t)p;
    p = realloc(p, new_size);

    if (p == NULL)
    {
        fprintf(stderr, "realloc() failed (out of memory)\n");
#if defined(CLITHER_BACKTRACE)
        print_backtrace(); /* probably won't work but may as well*/
#endif
        return NULL;
    }

    if (old_addr)
        track_deallocation(old_addr, "realloc()");
    track_allocation((uintptr_t)p, new_size);

    return p;
}

/* ------------------------------------------------------------------------- */
void mem_free(void* p)
{
    track_deallocation((uintptr_t)p, "free()");
    free(p);
}

int mem_deinit_threadlocal(void)
{
    uintptr_t leaks;

    /* report details on any g_allocations that were not de-allocated */
    uintptr_t           addr;
    struct report_info* info;
    int32_t             slot;
    hmap_for_each (state.report, slot, addr, info)
    {
        (void)addr;
        fprintf(
            stderr,
            "un-freed memory at 0x%" PRIx64 ", size 0x%" PRIx32 "\n",
            info->location,
            info->size);

#if defined(CLITHER_BACKTRACE)
        {
            int i;
            fprintf(stderr, "Backtrace:\n");
            for (i = BACKTRACE_OMIT_COUNT; i < info->backtrace_size; ++i)
            {
                if (strstr(info->backtrace[i], "invoke_main"))
                    break;
                fprintf(stderr, "  %s\n", info->backtrace[i]);
            }
        }
        backtrace_free(
            info->backtrace); /* this was allocated when malloc() was called */
#endif

#if defined(CLITHER_HEX_DUMP)
        if (info->size <= CLITHER_HEX_DUMP_SIZE)
            log_hex_ascii((void*)info->location, info->size);
#endif
    }

    state.ignore_malloc = 1;
    report_hmap_deinit(state.report);
    state.ignore_malloc = 0;

    /* overall report */
    leaks =
        (state.allocations > state.deallocations
             ? state.allocations - state.deallocations
             : state.deallocations - state.allocations);
    if (leaks)
    {
        fprintf(stderr, "Memory report:\n");
        fprintf(stderr, "  allocations   : %" PRIu32 "\n", state.allocations);
        fprintf(stderr, "  deallocations : %" PRIu32 "\n", state.deallocations);
        fprintf(
            stderr,
            COL_B_RED "  memory leaks  : %" PRIu64 COL_RESET "\n",
            leaks);
    }

    return (int)leaks;
}

void mem_track_allocation(void* p)
{
    track_allocation((uintptr_t)p, 1);
}

void mem_track_deallocation(void* p)
{
    track_deallocation((uintptr_t)p, "track_deallocation()");
}

void mem_own(void* p, int size)
{
    acquire((uintptr_t)p, size);
}

int mem_unown(void* p)
{
    return release((uintptr_t)p);
}
