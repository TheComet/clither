#include "clither/backtrace.h"
#include "clither/cli_colors.h"
#include "clither/hash.h"
#include "clither/hm.h"
#include "clither/log.h"
#include "clither/mem.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKTRACE_OMIT_COUNT 2

struct report_info
{
    uintptr_t location;
    int       size;
#if defined(CLITHER_BACKTRACE)
    int    backtrace_size;
    char** backtrace;
#endif
};

HM_DECLARE_HASH(report_hm, hash32, uintptr_t, struct report_info, 32)
static int report_hm_kvs_alloc(
    struct report_hm_kvs* kvs, struct report_hm_kvs* old_kvs, int32_t capacity)
{
    (void)old_kvs;
    if ((kvs->keys = (uintptr_t*)mem_alloc(sizeof(uintptr_t) * capacity)) ==
        ((void*)0))
        return -1;
    if ((kvs->values = (struct report_info*)mem_alloc(
             sizeof(struct report_info) * capacity)) == ((void*)0))
    {
        mem_free(kvs->keys);
        return -1;
    }
    return 0;
}
static void report_hm_kvs_free(struct report_hm_kvs* kvs)
{
    mem_free(kvs->values);
    mem_free(kvs->keys);
}
static void report_hm_kvs_free_old(struct report_hm_kvs* kvs)
{
    report_hm_kvs_free(kvs);
}
static uintptr_t
report_hm_kvs_get_key(const struct report_hm_kvs* kvs, int32_t slot)
{
    return kvs->keys[slot];
}
static void
report_hm_kvs_set_key(struct report_hm_kvs* kvs, int32_t slot, uintptr_t key)
{
    kvs->keys[slot] = key;
}
static int report_hm_kvs_keys_equal(uintptr_t k1, uintptr_t k2)
{
    return k1 == k2;
}
static struct report_info*
report_hm_kvs_get_value(const struct report_hm_kvs* kvs, int32_t slot)
{
    return &kvs->values[slot];
}
static void report_hm_kvs_set_value(
    struct report_hm_kvs* kvs, int32_t slot, const struct report_info* value)
{
    kvs->values[slot] = *value;
}
void report_hm_deinit(struct report_hm* hm)
{
    hash32 (*hash)(uintptr_t) = hash32_aligned_ptr;
    int (*storage_alloc)(
        struct report_hm_kvs*, struct report_hm_kvs*, int32_t) =
        report_hm_kvs_alloc;
    void (*storage_free_old)(struct report_hm_kvs*) = report_hm_kvs_free_old;
    void (*storage_free)(struct report_hm_kvs*) = report_hm_kvs_free;
    uintptr_t (*get_key)(const struct report_hm_kvs*, int32_t) =
        report_hm_kvs_get_key;
    void (*set_key)(struct report_hm_kvs*, int32_t, uintptr_t) =
        report_hm_kvs_set_key;
    int (*keys_equal)(uintptr_t, uintptr_t) = report_hm_kvs_keys_equal;
    struct report_info* (*get_value)(const struct report_hm_kvs*, int32_t) =
        report_hm_kvs_get_value;
    void (*set_value)(
        struct report_hm_kvs*, int32_t, const struct report_info*) =
        report_hm_kvs_set_value;
    (void)hash;
    (void)storage_alloc;
    (void)storage_free_old;
    (void)storage_free;
    (void)get_key;
    (void)set_key;
    (void)keys_equal;
    (void)get_value;
    (void)set_value;
    if (hm != ((void*)0))
    {
        report_hm_kvs_free(&hm->kvs);
        mem_free(hm);
    }
}
static int32_t
report_hm_find_slot(const struct report_hm* hm, uintptr_t key, hash32 h);
static int report_hm_grow(struct report_hm** hm)
{
    struct report_hm* new_hm;
    int32_t           i, header, data;
    int32_t           old_cap = *hm ? (*hm)->capacity : 0;
    int32_t           new_cap = old_cap ? old_cap * 2 : 128;
    ((void)sizeof(((new_cap & (new_cap - 1)) == 0) ? 1 : 0), __extension__({
         if ((new_cap & (new_cap - 1)) == 0)
             ;
         else
             __assert_fail(
                 "(new_cap & (new_cap - 1)) == 0",
                 "/home/thecomet/documents/programming/cpp/clither/src/mem.c",
                 27,
                 __extension__ __PRETTY_FUNCTION__);
     }));
    header = __builtin_offsetof(struct report_hm, hashes);
    data = sizeof((*hm)->hashes[0]) * new_cap;
    new_hm = (struct report_hm*)mem_alloc(header + data);
    if (new_hm == ((void*)0))
        goto alloc_hm_failed;
    if (report_hm_kvs_alloc(&new_hm->kvs, &(*hm)->kvs, new_cap) != 0)
        goto alloc_storage_failed;
    memset(new_hm->hashes, 0, sizeof(hash32) * new_cap);
    new_hm->count = 0;
    new_hm->capacity = new_cap;
    for (i = 0; i != old_cap; ++i)
    {
        int32_t slot;
        hash32  h;
        if ((*hm)->hashes[i] == 0 || (*hm)->hashes[i] == 1)
            continue;
        h = hash32_aligned_ptr(report_hm_kvs_get_key(&(*hm)->kvs, i));
        if (h == 0 || h == 1)
            h = 2;
        slot = report_hm_find_slot(
            new_hm, report_hm_kvs_get_key(&(*hm)->kvs, i), h);
        ((void)sizeof((slot >= 0) ? 1 : 0), __extension__({
             if (slot >= 0)
                 ;
             else
                 __assert_fail(
                     "slot >= 0",
                     "/home/thecomet/documents/programming/cpp/clither/src/"
                     "mem.c",
                     27,
                     __extension__ __PRETTY_FUNCTION__);
         }));
        new_hm->hashes[slot] = h;
        report_hm_kvs_set_key(
            &new_hm->kvs, slot, report_hm_kvs_get_key(&(*hm)->kvs, i));
        report_hm_kvs_set_value(
            &new_hm->kvs, slot, report_hm_kvs_get_value(&(*hm)->kvs, i));
        new_hm->count++;
    }
    if (*hm != ((void*)0))
    {
        report_hm_kvs_free_old(&(*hm)->kvs);
        mem_free(*hm);
    }
    *hm = new_hm;
    return 0;
alloc_storage_failed:
    mem_free(new_hm);
alloc_hm_failed:
    return log_oom(header + data, "hm_grow()");
}
static int32_t
report_hm_find_slot(const struct report_hm* hm, uintptr_t key, hash32 h)
{
    int32_t slot, i, last_rip;
    ((void)sizeof((hm && hm->capacity > 0) ? 1 : 0), __extension__({
         if (hm && hm->capacity > 0)
             ;
         else
             __assert_fail(
                 "hm && hm->capacity > 0",
                 "/home/thecomet/documents/programming/cpp/clither/src/mem.c",
                 27,
                 __extension__ __PRETTY_FUNCTION__);
     }));
    ((void)sizeof((h > 1) ? 1 : 0), __extension__({
         if (h > 1)
             ;
         else
             __assert_fail(
                 "h > 1",
                 "/home/thecomet/documents/programming/cpp/clither/src/mem.c",
                 27,
                 __extension__ __PRETTY_FUNCTION__);
     }));
    i = 0;
    last_rip = -1;
    slot = (int32_t)(h & (hash32)(hm->capacity - 1));
    while (hm->hashes[slot] != 0)
    {
        if (hm->hashes[slot] == h)
            if (report_hm_kvs_keys_equal(
                    report_hm_kvs_get_key(&hm->kvs, slot), key))
                return -(1 + slot);
        if (hm->hashes[slot] == 1)
            last_rip = slot;
        i++;
        slot = (int32_t)((slot + i) & (hm->capacity - 1));
    }
    if (last_rip != -1)
        slot = last_rip;
    return slot;
}
struct report_info* report_hm_emplace_new(struct report_hm** hm, uintptr_t key)
{
    hash32  h;
    int32_t slot;
    if (!*hm || (*hm)->count * 100 >= 70 * (*hm)->capacity)
        if (report_hm_grow(hm) != 0)
            return ((void*)0);
    h = hash32_aligned_ptr(key);
    if (h == 0 || h == 1)
        h = 2;
    slot = report_hm_find_slot(*hm, key, h);
    if (slot < 0)
        return ((void*)0);
    (*hm)->count++;
    (*hm)->hashes[slot] = h;
    report_hm_kvs_set_key(&(*hm)->kvs, slot, key);
    return report_hm_kvs_get_value(&(*hm)->kvs, slot);
}
enum hm_status report_hm_emplace_or_get(
    struct report_hm** hm, uintptr_t key, struct report_info** value)
{
    hash32  h;
    int32_t slot;
    if (!*hm || (*hm)->count * 100 >= 70 * (*hm)->capacity)
        if (report_hm_grow(hm) != 0)
            return HM_OOM;
    h = hash32_aligned_ptr(key);
    if (h == 0 || h == 1)
        h = 2;
    slot = report_hm_find_slot(*hm, key, h);
    if (slot < 0)
    {
        *value = report_hm_kvs_get_value(&(*hm)->kvs, -1 - slot);
        return HM_EXISTS;
    }
    (*hm)->count++;
    (*hm)->hashes[slot] = h;
    report_hm_kvs_set_key(&(*hm)->kvs, slot, key);
    *value = report_hm_kvs_get_value(&(*hm)->kvs, slot);
    return HM_NEW;
}
struct report_info* report_hm_find(const struct report_hm* hm, uintptr_t key)
{
    hash32  h;
    int32_t slot;
    if (hm == ((void*)0))
        return ((void*)0);
    h = hash32_aligned_ptr(key);
    if (h == 0 || h == 1)
        h = 2;
    slot = report_hm_find_slot(hm, key, h);
    if (slot >= 0)
        return ((void*)0);
    return report_hm_kvs_get_value(&hm->kvs, -1 - slot);
}
struct report_info* report_hm_erase(struct report_hm* hm, uintptr_t key)
{
    hash32  h;
    int32_t slot;
    h = hash32_aligned_ptr(key);
    if (h == 0 || h == 1)
        h = 2;
    slot = report_hm_find_slot(hm, key, h);
    if (slot >= 0)
        return ((void*)0);
    hm->count--;
    hm->hashes[-1 - slot] = 1;
    return report_hm_kvs_get_value(&hm->kvs, -1 - slot);
}

struct state
{
    struct report_hm* report;
    int               allocations;
    int               deallocations;
    unsigned          ignore_malloc : 1;
};

static CLITHER_THREADLOCAL struct state state;

void mem_init_threadlocal(void)
{
    state.allocations = 0;
    state.deallocations = 0;

    report_hm_init(&state.report);
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
    }

    if (state.ignore_malloc)
        return;

    /* insert info into hashmap */
    state.ignore_malloc = 1;
    info = report_hm_emplace_new(&state.report, addr);
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
    }

    if (state.ignore_malloc)
        return;

    /* find matching allocation and remove from hashmap */
    info = report_hm_erase(state.report, addr);
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
    info = report_hm_emplace_new(&state.report, addr);
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
    info = report_hm_erase(state.report, addr);
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

/* ------------------------------------------------------------------------- */
static void log_hex_ascii(const void* data, int len)
{
    int i;

    for (i = 0; i != 16; ++i)
        fprintf(stderr, "%c  ", "0123456789ABCDEF"[i]);
    putc(' ', stderr);
    for (i = 0; i != 16; ++i)
        putc("0123456789ABCDEF"[i], stderr);
    putc('\n', stderr);

    for (i = 0; i < len;)
    {
        int     j;
        uint8_t c = ((const uint8_t*)data)[i];
        for (j = 0; j != 16; ++j)
        {
            if (i + j < len)
                fprintf(stderr, "%02x ", c);
            else
                fprintf(stderr, "   ");
        }

        fprintf(stderr, " ");
        for (j = 0; j != 16 && i + j != len; ++j)
        {
            if (c >= 32 && c < 127) /* printable ascii */
                putc(c, stderr);
            else
                putc('.', stderr);
        }

        fprintf(stderr, "\n");
        i += 16;
    }
}

int mem_deinit_threadlocal(void)
{
    uintptr_t leaks;

    /* report details on any g_allocations that were not de-allocated */
    uintptr_t           addr;
    struct report_info* info;
    int32_t             slot;
    hm_for_each (state.report, slot, addr, info)
    {
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

#if defined(CLITHER_MEM_HEX_DUMP)
        if (info->size <= CLITHER_MEM_HEX_DUMP_SIZE)
            log_hex_ascii((void*)info->location, info->size);
#endif
    }

    state.ignore_malloc = 1;
    report_hm_deinit(state.report);
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
