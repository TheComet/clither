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

#include "clither/config.h"
#include "clither/log.h" /* log_oom */
#include "clither/mem.h" /* mem_alloc, mem_realloc, mem_free */
#include <stddef.h>      /* offsetof */
#include <string.h>      /* memmove */

#define RB_DECLARE(prefix, T, bits) RB_DECLARE_FULL(prefix, T, bits, 16)

#define RB_DECLARE_FULL(prefix, T, bits, MIN_CAPACITY)                         \
    struct prefix                                                              \
    {                                                                          \
        int##bits##_t read, write, capacity;                                   \
        T             data[1];                                                 \
    };                                                                         \
                                                                               \
    /*!                                                                        \
     * @brief This must be called before operating on any ring buffer.         \
     * Initializes the structure to a defined state.                           \
     * @param[in] rb Pointer to a ring buffer of type RB(T,B)*                 \
     */                                                                        \
    static void prefix##_init(struct prefix** rb)                              \
    {                                                                          \
        *rb = NULL;                                                            \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Destroys an existing ring buffer object and frees all memory     \
     * allocated by inserting elements.                                        \
     * @param[in] rb Pointer to a ring buffer of type RB(T,B)*                 \
     */                                                                        \
    void prefix##_deinit(struct prefix* rb);                                   \
                                                                               \
    /*!                                                                        \
     * @brief Resizes the ring buffer to contain N number of slots.            \
     * @note Only N-1 elements can be inserted before the buffer is full.      \
     * @param[in] rb Pointer to a ring buffer of type RB(T,B)*                 \
     * @param[in] slots Number of slots to resize to. @note Value must be a    \
     * power of 2.                                                             \
     * @return Returns 0 on success, negative on error.                        \
     */                                                                        \
    int prefix##_resize(struct prefix** rb, int##bits##_t elems);              \
                                                                               \
    /*!                                                                        \
     * @brief Adds (copies) an element into the writing-end of the ring        \
     * buffer. The write pointer is advanced to the next position.             \
     * @note If the buffer is full, this function will fail. If you want to    \
     * automatically resize the buffer if it is full, use @see                 \
     * rb_put_resize().                                                        \
     * @note If you do not wish to copy data into the ring buffer, but merely  \
     * make space, use @see rb_emplace() instead.                              \
     * @param[in] rb Pointer to a ring buffer of type RB(T,B)*                 \
     * @param[in] elem The element to copy into the ring buffer.               \
     * @return Returns 0 on success, or negative if the buffer was full.       \
     */                                                                        \
    static int prefix##_put(struct prefix* rb, T elem)                         \
    {                                                                          \
        int##bits##_t write;                                                   \
        CLITHER_DEBUG_ASSERT(rb);                                              \
        write = rb->write;                                                     \
        if (rb_is_full(rb))                                                    \
            return -1;                                                         \
        rb->write = (write + 1) & ((int##bits##_t)rb->capacity - 1);           \
        rb->data[write] = elem;                                                \
        return 0;                                                              \
    }                                                                          \
    static int prefix##_put_realloc(struct prefix** rb, T elem)                \
    {                                                                          \
        int##bits##_t write;                                                   \
        if (*rb == NULL || rb_is_full(*rb))                                    \
            if (prefix##_resize(                                               \
                    rb, *rb ? (*rb)->capacity * 2 : MIN_CAPACITY) != 0)        \
            {                                                                  \
                return -1;                                                     \
            }                                                                  \
        write = (*rb)->write;                                                  \
        (*rb)->write = (write + 1) & ((int##bits##_t)(*rb)->capacity - 1);     \
        (*rb)->data[write] = elem;                                             \
        return 0;                                                              \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Allocates space for a new element at the writing-end of the ring \
     * buffer, but does not initialize it.                                     \
     * @warning The returned pointer could be invalidated if the buffer is     \
     * resized.                                                                \
     * @note If the buffer is full, this function will fail. If you want a     \
     * version of this function that automatically resizes the buffer in this  \
     * case, use @see rb_emplace_realloc() instead.                            \
     * @param[in] rb Pointer to a ring buffer of type RB(T,B)*                 \
     * @return Returns a pointer to the uninitialized element if insertion is  \
     * successfull, or NULL if there is no space left.                         \
     */                                                                        \
    static T* prefix##_emplace(struct prefix* rb)                              \
    {                                                                          \
        int##bits##_t write = rb->write;                                       \
        T*            value = &rb->data[write];                                \
        if (rb_is_full(rb))                                                    \
            return NULL;                                                       \
        rb->write = (write + 1) & ((int##bits##_t)rb->capacity - 1);           \
        return value;                                                          \
    }                                                                          \
    static T* prefix##_emplace_realloc(struct prefix** rb)                     \
    {                                                                          \
        int##bits##_t write;                                                   \
        T*            value;                                                   \
        if (*rb == NULL || rb_is_full(*rb))                                    \
            if (prefix##_resize(                                               \
                    rb, *rb ? (*rb)->capacity * 2 : MIN_CAPACITY) != 0)        \
            {                                                                  \
                return NULL;                                                   \
            }                                                                  \
        write = (*rb)->write;                                                  \
        value = &(*rb)->data[write];                                           \
        (*rb)->write = (write + 1) & ((int##bits##_t)(*rb)->capacity - 1);     \
        return value;                                                          \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Removes an element from the reading-end of the ring buffer and   \
     * returns it.                                                             \
     * @warning The ring buffer must contain at least 1 element.               \
     * Use @see rb_is_empty() first if you need to.                            \
     * @param[in] rb Pointer to a ring buffer of type RB(T,B)*                 \
     */                                                                        \
    static T prefix##_take(struct prefix* rb)                                  \
    {                                                                          \
        int##bits##_t read;                                                    \
        T             data;                                                    \
        CLITHER_DEBUG_ASSERT(!rb_is_empty(rb));                                \
        read = rb->read;                                                       \
        data = rb->data[read];                                                 \
        rb->read = (read + 1) & ((int##bits##_t)rb->capacity - 1);             \
        return data;                                                           \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Removes an element from the writing-end of the ring buffer and   \
     * returns it.                                                             \
     * @warning The ring buffer must contain at least 1 element.               \
     * Use @see rb_is_empty() first if you need to.                            \
     * @param[in] rb Pointer to a ring buffer of type RB(T,B)*                 \
     */                                                                        \
    static T prefix##_takew(struct prefix* rb)                                 \
    {                                                                          \
        int##bits##_t write;                                                   \
        T             data;                                                    \
        CLITHER_DEBUG_ASSERT(!rb_is_empty(rb));                                \
        write = (rb->write - 1) & ((int##bits##_t)rb->capacity - 1);           \
        data = rb->data[write];                                                \
        rb->write = write;                                                     \
        return data;                                                           \
    }                                                                          \
                                                                               \
    static void prefix##_clear(struct prefix* rb)                              \
    {                                                                          \
        CLITHER_DEBUG_ASSERT(rb);                                              \
        rb->read = rb->write;                                                  \
    }

#define IS_POWER_OF_2(x) (((x) & ((x) - 1)) == 0)
#define RB_DEFINE(prefix, T, bits)                                             \
    void prefix##_deinit(struct prefix* rb)                                    \
    {                                                                          \
        if (rb)                                                                \
            mem_free(rb);                                                      \
    }                                                                          \
    int prefix##_resize(struct prefix** rb, int##bits##_t elems)               \
    {                                                                          \
        struct prefix* new_rb;                                                 \
        int            header = offsetof(struct prefix, data);                 \
        int            data = sizeof(T) * elems;                               \
                                                                               \
        CLITHER_DEBUG_ASSERT(IS_POWER_OF_2(elems));                            \
        new_rb = (struct prefix*)mem_realloc(*rb, header + data);              \
        if (new_rb == NULL)                                                    \
            return log_oom(header + data, "rb_resize()");                      \
        if (*rb == NULL)                                                       \
        {                                                                      \
            (*rb) = new_rb;                                                    \
            (*rb)->read = 0;                                                   \
            (*rb)->write = 0;                                                  \
        }                                                                      \
        else                                                                   \
            (*rb) = new_rb;                                                    \
                                                                               \
        /* Is the data wrapped? */                                             \
        if ((*rb)->read > (*rb)->write)                                        \
        {                                                                      \
            memmove(                                                           \
                (*rb)->data + (*rb)->capacity,                                 \
                (*rb)->data,                                                   \
                (*rb)->write * sizeof(T));                                     \
            (*rb)->write += (*rb)->capacity;                                   \
        }                                                                      \
                                                                               \
        (*rb)->capacity = elems;                                               \
        return 0;                                                              \
    }

#define rb_count(rb)                                                           \
    ((rb) ? ((rb)->write - (rb)->read) & ((rb)->capacity - 1) : 0)

#define rb_space(rb)                                                           \
    ((rb) ? ((rb)->read - (rb)->write - 1) & ((rb)->capacity - 1) : 0)

#define rb_is_full(rb)                                                         \
    ((rb) ? (((rb)->write + 1) & ((rb)->capacity - 1)) == (rb)->read : 1)

#define rb_is_empty(rb) ((rb) ? (rb)->read == (rb)->write : 1)

#define rb_peek(rb, idx)                                                       \
    (CLITHER_DEBUG_ASSERT(                                                     \
         (rb) && (rb)->read != (rb)->write && (idx) >= 0 &&                    \
         (idx) < (((rb)->write - (rb)->read) & ((rb)->capacity - 1))),         \
     &(rb)->data[((rb)->read + (idx)) & ((rb)->capacity - 1)])

#define rb_peek_read(rb)                                                       \
    (CLITHER_DEBUG_ASSERT((rb) && (rb)->read != (rb)->write),                  \
     &(rb)->data[(rb)->read])

#define rb_peek_write(rb)                                                      \
    (CLITHER_DEBUG_ASSERT((rb) && (rb)->read != (rb)->write),                  \
     &(rb)->data[((rb)->write - 1) & ((rb)->capacity - 1)])

/*!
 * @brief Iterates over the elements in a ring buffer.
 *
 *   T* element;
 *   RB(T,32) rb = some_ring_buffer();
 *   rb_for_each(rb, element) {
 *       ... do stuff ...
 *   }
 */
#define rb_for_each(rb, i, elem)                                               \
    for (i = (rb) ? (rb)->read : 0;                                            \
         i != ((rb) ? (rb)->write : 0) && ((elem = &(rb)->data[i]) || 1);      \
         i = (i + 1) & ((rb)->capacity - 1))
