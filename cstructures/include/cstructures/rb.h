/*!
 * @file rb.h
 * @author TheComet
 *
 * The ring buffer consists of a read and write index, and a chunk of memory:
 * ```c
 * struct rb_t {
 *     int read, write;
 *     T data[N];
 * }
 * ```
 *
 * The buffer is considered empty when rb->read == rb->write. It is considered
 * full when rb->write is one slot behind rb->read. This is necessary because
 * otherwise there would be no way to tell the difference between an empty and
 * a full ring buffer.
 */
#pragma once

#include "cstructures/config.h"
#include <stdint.h>

C_BEGIN

/*
 * For the following 6 macros it is very important that each argument is only
 * accessed once.
 */

#define RB_COUNT_N(rb, N)                                        \
        (((rb)->write - (rb)->read) & ((N)-1u))

#define RB_SPACE_N(rb, N)                                        \
        (((rb)->read - (rb)->write - 1) & ((N)-1u))

#define RB_IS_FULL_N(rb, N)                                      \
        ((rb_idx)(((rb)->write + 1u) & ((N)-1u)) == (rb)->read)

#define RB_IS_EMPTY_N(rb, N)                                     \
        ((rb)->read == (rb)->write)

#define RB_COUNT_TO_END_N(result, rb, N) {                       \
            rb_idx end = (rb_idx)((N) - (rb)->read);             \
            rb_idx n = (rb_idx)((rb)->write + end) & ((N)-1u);   \
            result = (rb_idx)(n < end ? n : end);                \
        }

#define RB_SPACE_TO_END_N(result, rb, N) {                       \
            rb_idx end = (rb_idx)(((N)-1u) - (rb)->write);       \
            rb_idx n = (rb_idx)(end + (rb)->read) & ((N)-1u);    \
            result = n <= end ? n : end + 1u;                    \
        }

typedef int16_t rb_idx;
typedef uint16_t rb_size;

struct cs_rb
{
    char* buffer;
    rb_size value_size;
    rb_size capacity;
    rb_idx read, write;
};

CSTRUCTURES_PUBLIC_API void
rb_init(struct cs_rb* rb, rb_size value_size);

CSTRUCTURES_PUBLIC_API void
rb_deinit(struct cs_rb* rb);

CSTRUCTURES_PUBLIC_API int
rb_realloc(struct cs_rb* rb, rb_size new_size);

CSTRUCTURES_PUBLIC_API int
rb_put(struct cs_rb* rb, const void* data);

CSTRUCTURES_PUBLIC_API void*
rb_emplace(struct cs_rb* rb);

CSTRUCTURES_PUBLIC_API void*
rb_take(struct cs_rb* rb);

#define rb_clear(rb) \
    (rb)->read = (rb)->write

#define rb_peek_read(rb) \
    (void*)((rb)->buffer + (rb)->read * (rb)->value_size)

#define rb_peek_write(rb) \
    (void*)((rb)->buffer + (((rb)->write - 1) & ((rb)->capacity - 1)) * (rb)->value_size)

#define rb_peek(rb, idx) \
    (void*)((rb)->buffer + (((rb)->read + (idx)) & ((rb)->capacity - 1)) * (rb)->value_size)

#define rb_count(rb) \
    RB_COUNT_N(rb, (rb)->capacity)

#define rb_is_full(rb) \
    RB_IS_FULL_N(rb, (rb)->capacity)

#define rb_is_empty(rb) \
    RB_IS_EMPTY_N(rb, (rb)->capacity)

#define RB_FOR_EACH(rb, var_type, var) {                                      \
    var_type* var;                                                            \
    rb_idx var##_i;                                                           \
    for(var##_i = (rb)->read,                                                 \
        var = (var_type*)((rb)->buffer + (rb)->read * (rb)->value_size);      \
        var##_i != (rb)->write;                                               \
        var##_i = (var##_i + 1) & ((rb)->capacity - 1),                       \
        var = (var_type*)((rb)->buffer + var##_i * (rb)->value_size)) {

#define RB_END_EACH }}

C_END
