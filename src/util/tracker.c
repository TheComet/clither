#include "clither/platform/backtrace.h"
#include "clither/util/hmap.h"
#include "clither/util/mem.h"
#include "clither/util/tracker.h"
#include <inttypes.h>

struct data
{
    int size;
#if defined(CLITHER_BACKTRACE)
    int    backtrace_size;
    char** backtrace;
#endif
};

HMAP_DECLARE_HASH(static, tracker_hmap, hash32, uintptr_t, struct data, 32)
HMAP_DEFINE_HASH(
    static,
    tracker_hmap,
    hash32,
    uintptr_t,
    struct data,
    32,
    hash32_aligned_ptr)

struct tracker
{
    struct tracker_hmap* hmap;
    int                  tracks, untracks;
};

/* ------------------------------------------------------------------------- */
static void print_backtrace(const struct data* data)
{
    int i;
    for (i = BACKTRACE_OMIT_COUNT; i < data->backtrace_size; ++i)
    {
        if (strstr(data->backtrace[i], "invoke_main"))
            break;
        log_raw("  %s\n", data->backtrace[i]);
    }
}

/* ------------------------------------------------------------------------- */
struct tracker* tracker_create(void)
{
    struct tracker* t = mem_alloc(sizeof *t);
    if (t == NULL)
        return NULL;

    tracker_hmap_init(&t->hmap);
    return t;
}

/* ------------------------------------------------------------------------- */
int tracker_destroy(struct tracker* t)
{
    int          slot;
    uintptr_t    p;
    struct data* data;

    hmap_for_each (t->hmap, slot, p, data)
    {
        (void)slot;
        log_err("Un-freed resource 0x%" PRIxPTR ", size %d\n", p, data->size);
#if defined(CLITHER_BACKTRACE)
        print_backtrace(data);
        backtrace_free(data->backtrace);
#endif
#if defined(CLITHER_HEX_DUMP)
        if (data->size <= CLITHER_HEX_DUMP_SIZE)
            log_hex_ascii((void*)p, data->size);
#endif
    }

#if defined(CLITHER_BACKTRACE)
    if (t->tracks != t->untracks)
    {
        log_err("Call to tracker_destroy():\n");
        log_backtrace();
    }
#endif

    tracker_hmap_deinit(t->hmap);
    mem_free(t);

    return t->tracks - t->untracks;
}

/* ------------------------------------------------------------------------- */
void tracker_track(struct tracker* t, void* p, int size)
{
    struct data* data;
    ++t->tracks;

    switch (tracker_hmap_emplace_or_get(&t->hmap, (uintptr_t)p, &data))
    {
        case HMAP_OOM: break;
        case HMAP_NEW: {
            data->size = size;
#if defined(CLITHER_BACKTRACE)
            data->backtrace = backtrace_get(&data->backtrace_size);
#endif
            break;
        }

        case HMAP_EXISTS: {
            log_err(
                "Double track! This is usually caused by calling "
                "tracker_track() on the same resource twice.\n");
#if defined(CLITHER_BACKTRACE)
            log_backtrace();
            log_err("Resource was previously tracked at:\n");
            print_backtrace(data);
#endif
            break;
        }
    }
}

/* ------------------------------------------------------------------------- */
int tracker_untrack(struct tracker* t, void* p)
{
    struct data* data;
    ++t->untracks;

    data = tracker_hmap_erase(t->hmap, (uintptr_t)p);
    if (data == NULL)
    {
        log_err("Untracking a resource that was never tracked.\n");
#if defined(CLITHER_BACKTRACE)
        log_backtrace();
#endif
        return 0;
    }

#if defined(CLITHER_BACKTRACE)
    if (data->backtrace)
        backtrace_free(data->backtrace);
#endif
    return data->size;
}
