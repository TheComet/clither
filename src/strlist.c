#include "clither/log.h"
#include "clither/mem.h"
#include "clither/strlist.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

#define EXTRA_PADDING 1 /* NULL terminator */

/*
 *                capacity
 *  |<-------------------------------->|
 *    str_used
 *  |<------>|
 * [aaaaabbbbb........[off|len][off|len]]
 * ----------->     <------------------
 * Strings grow upwards, refs grow downwards
 *
 */
static int
grow(struct strlist** l, int str_len)
{
    int count = *l ? (*l)->count : 0;
    int old_table_size = sizeof(struct strspan) * count;
    int new_table_size = sizeof(struct strspan) * (count + 1);

    while ((*l ? (*l)->capacity : 0) < new_table_size
                                           + (*l ? (*l)->str_used : 0) + str_len
                                           + EXTRA_PADDING)
    {
        int   cap = *l ? (*l)->capacity : 0;
        int   grow_size = cap ? cap : 128;
        int   struct_header = offsetof(struct strlist, data);
        void* new_mem = mem_realloc(*l, cap + grow_size + struct_header);
        if (new_mem == NULL)
            return log_oom(cap + grow_size + struct_header, "strlist_grow()");
        *l = new_mem;
        if (cap == 0)
        {
            (*l)->count = 0;
            (*l)->str_used = 0;
            (*l)->capacity = 0;
        }
        (*l)->capacity += grow_size;

        memmove(
            (*l)->data + (*l)->capacity - old_table_size,
            (*l)->data + (*l)->capacity - old_table_size - grow_size,
            old_table_size);
    }

    return 0;
}

void
strlist_deinit(struct strlist* l)
{
    if (l)
        mem_free(l);
}

#if defined(ODBUTIL_MEM_DEBUGGING)
void
mem_acquire_strlist(struct strlist* l)
{
    if (l == NULL)
        return;
    mem_acquire(l, offsetof(struct strlist, data) + l->capacity);
}
void
mem_release_strlist(struct strlist* l)
{
    mem_release(l);
}
#endif

int
strlist_add(struct strlist** l, struct strview str)
{
    struct strspan* ref;

    if (grow(l, str.len) < 0)
        return -1;

    ref = &STRLIST_TABLE_PTR(*l)[-(*l)->count];
    ref->off = (*l)->str_used;
    ref->len = str.len;

    memcpy((*l)->data + ref->off, str.data + str.off, str.len);

    (*l)->str_used += str.len + EXTRA_PADDING;
    (*l)->count++;

    return 0;
}

int
strlist_insert(struct strlist** l, int insert, struct strview in)
{
    struct strspan* slotspan;

    if (grow(l, in.len) < 0)
        return -1;

    slotspan = &STRLIST_TABLE_PTR(*l)[-insert];
    if (insert < (*l)->count)
    {
        struct strspan* span;
        int             i;

        /* Move strings to make space for str.len+padding */
        memmove(
            (*l)->data + slotspan->off + in.len + EXTRA_PADDING,
            (*l)->data + slotspan->off,
            (*l)->str_used - slotspan->off);

        /* Move span table */
        memmove(
            (struct strspan*)((*l)->data + (*l)->capacity) - (*l)->count - 1,
            (struct strspan*)((*l)->data + (*l)->capacity) - (*l)->count,
            sizeof(struct strspan) * ((*l)->count - insert));

        /* Calculate new offsets */
        for (span = slotspan - 1, i = (*l)->count - insert; i; i--, span--)
            span->off += in.len + EXTRA_PADDING;
    }
    else
    {
        slotspan->off = (*l)->str_used;
    }

    memcpy((*l)->data + slotspan->off, in.data + in.off, in.len);
    slotspan->len = in.len;

    (*l)->str_used += in.len + EXTRA_PADDING;
    (*l)->count++;

    return 0;
}

void
strlist_erase(struct strlist* l, int idx)
{
    struct strspan* span = &STRLIST_TABLE_PTR(l)[-idx];
    int             str_gap = span->len + EXTRA_PADDING;
    if (idx < l->count - 1)
    {
        int i;

        /* Move strings to fill gap */
        memmove(
            l->data + span->off,
            l->data + span->off + str_gap,
            l->str_used - span->off - str_gap);

        /* Move span table to fill gap */
        memmove(
            (struct strspan*)(l->data + l->capacity) - l->count + 1,
            (struct strspan*)(l->data + l->capacity) - l->count,
            sizeof(struct strspan) * (l->count - idx - 1));

        /* Calculate new offsets */
        for (i = l->count - idx - 1; i; i--, span--)
            span->off -= str_gap;
    }

    l->str_used -= str_gap;
    l->count--;
}

static int
lexicographically_less(struct strview s1, struct strview s2)
{
    int cmp = memcmp(
        s1.data + s1.off, s2.data + s2.off, s1.len < s2.len ? s1.len : s2.len);
    if (cmp == 0)
        return s1.len < s2.len;
    return cmp < 0;
}

int
strlist_lower_bound(const struct strlist* l, struct strview str)
{
    int half, middle, found, len;

    found = 0;
    len = l ? l->count : 0;

    while (len)
    {
        half = len / 2;
        middle = found + half;
        if (lexicographically_less(strlist_view(l, middle), str))
        {
            found = middle;
            ++found;
            len = len - half - 1;
        }
        else
            len = half;
    }

    return found;
}

int
strlist_upper_bound(const struct strlist* l, struct strview str)
{
    int half, middle, found, len;

    found = 0;
    len = l ? l->count : 0;

    while (len)
    {
        half = len / 2;
        middle = found + half;
        if (lexicographically_less(str, strlist_view(l, middle)))
            len = half;
        else
        {
            found = middle;
            ++found;
            len = len - half - 1;
        }
    }

    return found;
}
