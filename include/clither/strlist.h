#pragma once

#include "clither/strspan.h"
#include "clither/strview.h"
#include <stddef.h> /* NULL */

#define STRLIST_TABLE_PTR(l) ((struct strspan*)((l)->data + (l)->capacity) - 1)

struct strlist
{
    int count;    /* Number of strings in list */
    int str_used; /* The total length of all strings concatenated */
    int capacity; /* Capacity in bytes of "data" (excluding the rest of the
                     struct). Realloc when str_len >= str_capacity */
    char data[1];
};

static void strlist_init(struct strlist** l)
{
    *l = NULL;
}

void strlist_deinit(struct strlist* l);

#if defined(ODBUTIL_MEM_DEBUGGING)
void mem_own_strlist(struct strlist* l);
void mem_unown_strlist(struct strlist* l);
#else
#    define mem_own_strlist(l)
#    define mem_unown_strlist(l)
#endif

int  strlist_add(struct strlist** l, struct strview str);
int  strlist_add_cstr(struct strlist** l, const char* cstr);
int  strlist_insert(struct strlist** l, int insert, const char* cstr);
void strlist_erase(struct strlist* l, int idx);

static struct strspan strlist_span(const struct strlist* l, int i)
{
    return STRLIST_TABLE_PTR(l)[-i];
}

static struct strview strlist_view(const struct strlist* l, int i)
{
    struct strspan span = strlist_span(l, i);
    return strview(l->data, span.off, span.len);
}

static const char* strlist_cstr(const struct strlist* l, int i)
{
    struct strspan span = strlist_span(l, i);
    return l->data + span.off;
}

static int strlist_count(const struct strlist* l)
{
    return l ? l->count : 0;
}

/*!
 * @brief Finds the first position in which a string could be inserted without
 * changing the ordering.
 *
 * 1) If cmd exists in the list, then a reference to that string is returned.
 * 2) If the string does not exist, then the first string that lexicographically
 *    compares less than the string being searched-for is returned.
 * 3) If there is no string that lexicographically compares less than the
 *    searched-for string, the returned reference will have length zero, but
 *    its offset will point after the last valid character in the list.
 *
 * @note The list must be sorted.
 * @note Algorithm based on GNU GCC stdlibc++'s lower_bound function,
 * line 2121 in stl_algo.h
 * https://gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-4.3/a02014.html
 */
int strlist_lower_bound(const struct strlist* l, struct strview str);

#define strlist_for_each(l, i, var)                                            \
    for (i = 0; (l) && i != (l)->count && ((var = strlist_cstr((l), i)), 1);   \
         ++i)
