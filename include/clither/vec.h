/*!
 * @file vector.h
 * @brief Dynamic contiguous sequence container with guaranteed element order.
 * @page vector Ordered Vector
 *
 * Ordered vectors arrange all inserted elements next to each other in memory.
 * Because of this, vector access is just as efficient as a normal array, but
 * they are able to grow and shrink in size automatically.
 */
#pragma once

#include "clither/config.h"
#include "clither/log.h" /* log_oom */
#include "clither/mem.h" /* mem_alloc, mem_realloc, mem_free */
#include <stddef.h>      /* offsetof */
#include <string.h>      /* memmove */

#define VEC_RETAIN 0
#define VEC_ERASE  1

#define VEC_DECLARE(prefix, T, bits)                                           \
    struct prefix                                                              \
    {                                                                          \
        int##bits##_t count, capacity;                                         \
        T             data[1];                                                 \
    };                                                                         \
                                                                               \
    /*!                                                                        \
     * @brief This must be called before operating on any vector. Initializes  \
     * the structure to a defined state.                                       \
     * @param[in] v Pointer to a vector of type VEC(T,B)*                      \
     */                                                                        \
    static inline void prefix##_init(struct prefix** v)                        \
    {                                                                          \
        *v = NULL;                                                             \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Destroys an existing vector object and frees all memory          \
     * allocated by inserted elements.                                         \
     * @param[in] v Vector of type VEC(T,B)                                    \
     */                                                                        \
    void prefix##_deinit(struct prefix* v);                                    \
                                                                               \
    /*!                                                                        \
     * @brief Grows or shrinks the vector's capacity to the specified size.    \
     * Excess elements are discarded if the new capacity is smaller. The       \
     * vector's "count" is clamped to the new capacity, meaning, if the new    \
     * capacity is smaller, then the count is reduced, but if the new capacity \
     * is larger, then the count remains the same.                             \
     */                                                                        \
    int prefix##_realloc(struct prefix** v, int##bits##_t new_capacity);       \
                                                                               \
    /*!                                                                        \
     * @brief Resets the vector's size to 0.                                   \
     * @note This does not actually free the underlying memory, it simply      \
     * resets the element counter. If you wish to free the underlying memory,  \
     * see vec_clear_compact().                                                \
     * @param[in] v Pointer to a vector of type VEC(T,B)*                      \
     */                                                                        \
    static inline void prefix##_clear(struct prefix* v)                        \
    {                                                                          \
        if (v)                                                                 \
            v->count = 0;                                                      \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Frees any excess memory. The memory buffer is resized to fit the \
     * current number of elements.                                             \
     * @param[in] v Pointer to a vector of type VEC(T,B)*                      \
     */                                                                        \
    void prefix##_compact(struct prefix** v);                                  \
                                                                               \
    /*!                                                                        \
     * @brief A combination of @see vec_clear() and @see vec_compact(). Resets \
     * the vector's count to 0 and frees all underlying memory.                \
     * @param[in] v Pointer to a vector of type VEC(T,B)*                      \
     */                                                                        \
    static inline void prefix##_clear_compact(struct prefix** v)               \
    {                                                                          \
        prefix##_clear(*v);                                                    \
        prefix##_compact(v);                                                   \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Allocates space for a new element at the end of the vector, but  \
     * does not initialize it.                                                 \
     * @warning The returned pointer could be invalidated if any other         \
     * vector related function is called, as the underlying memory of the      \
     * vector could be re-allocated. Use the pointer immediately after calling \
     * this function.                                                          \
     * @note This can cause a re-allocation of the underlying memory. This     \
     * implementation expands the allocated memory by a factor of 2 every time \
     * the capacity is reached to cut down on the frequency of re-allocations. \
     * @param[in] v Pointer to a vector of type VEC(T,B)*                      \
     * @param[in] element The element to copy into the vector.                 \
     * @return Returns a pointer to the inserted space.                        \
     */                                                                        \
    T* prefix##_emplace(struct prefix** v);                                    \
                                                                               \
    /*!                                                                        \
     * @brief Inserts (copies) a new element into the end of the vector.       \
     * @note This can cause a re-allocation of the underlying memory. This     \
     * implementation expands the allocated memory by a factor of 2 every time \
     * the capacity is reached to cut down on the frequency of re-allocations. \
     * @note If you do not wish to copy data into the vector, but merely make  \
     * space, see vec_push_emplace().                                          \
     * @param[in] v Pointer to a vector of type VEC(T,B)*                      \
     * @param[in] elem The element to copy into the vector.                    \
     * @return Returns 0 on success, negative on error.                        \
     */                                                                        \
    static inline int prefix##_push(struct prefix** v, T elem)                 \
    {                                                                          \
        T* ins = prefix##_emplace(v);                                          \
        if (ins == NULL)                                                       \
            return -1;                                                         \
        *ins = elem;                                                           \
        return 0;                                                              \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Allocates space for a new element at the specified index in the  \
     * vector, but does not initialize it.                                     \
     * @warning The returned pointer could be invalidated if any other         \
     * vector related function is called, as the underlying memory of the      \
     * vector could be re-allocated. Use the pointer immediately after calling \
     * this function.                                                          \
     * @note This can cause a re-allocation of the underlying memory. This     \
     * implementation expands the allocated memory by a factor of 2 every time \
     * the capacity is reached to cut down on the frequency of re-allocations. \
     * @param[in] v Pointer to a vector of type VEC(T,B)*                      \
     * @param[in] i The index the new element should be inserted into.         \
     * Existing elements are shifted aside to make space. Should be between 0  \
     * - vec_count().                                                          \
     * @return Returns a pointer to the inserted space.                        \
     */                                                                        \
    T* prefix##_insert_emplace(struct prefix** v, int##bits##_t i);            \
                                                                               \
    /*!                                                                        \
     * @brief Inserts (copies) a new element into the vector at the specified  \
     * index.                                                                  \
     * @note This can cause a re-allocation of the underlying memory. This     \
     * implementation expands the allocated memory by a factor of 2 every time \
     * the capacity is reached to cut down on the frequency of re-allocations. \
     * @note If you do not wish to copy data into the vector, but merely make  \
     * space, @see vec_insert_emplace().                                       \
     * @param[in] v Pointer to a vector of type VEC(T,B)*                      \
     * @param[in] i The index the new element should be inserted into.         \
     * Existing elements are shifted aside to make space. Should be between 0  \
     * to vec_count().                                                         \
     * @param[in] elem The element to copy into the vector.                    \
     * @return Returns 0 on success, negative on error.                        \
     */                                                                        \
    static inline int prefix##_insert(                                         \
        struct prefix** v, int##bits##_t i, T elem)                            \
    {                                                                          \
        T* ins = prefix##_insert_emplace(v, i);                                \
        if (ins == NULL)                                                       \
            return -1;                                                         \
        *ins = elem;                                                           \
        return 0;                                                              \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Removes an element from the end of the vector.                   \
     * @warning The vector must have at least 1 element. If you cannot         \
     * guarantee this, check @see vec_count() first.                           \
     * @warning The returned pointer could be invalidated if any other         \
     * vector related function is called, as the underlying memory of the      \
     * vector could be re-allocated. Use the pointer immediately after calling \
     * this function.                                                          \
     * @param[in] v Pointer to a vector of type VEC(T,B)*.                     \
     * @return A pointer to the popped element. See warning and use with       \
     * caution. Vector must not be empty.                                      \
     */                                                                        \
    static inline T* prefix##_pop(struct prefix* v)                            \
    {                                                                          \
        CLITHER_DEBUG_ASSERT(v->count > 0);                                    \
        return &v->data[--(v->count)];                                         \
    }                                                                          \
                                                                               \
    static inline T* prefix##_pop_by(struct prefix* v, int##bits##_t count)    \
    {                                                                          \
        return &v->data[v->count -= count];                                    \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Erases an element at the specified index from the vector.        \
     * @note This causes all elements with indices greater than **i** to be    \
     * shifted 1 element down so the vector remains contiguous.                \
     * @param[in] i The position of the element in the vector to erase. The    \
     * index ranges from 0 to vec_count()-1.                                   \
     */                                                                        \
    void prefix##_erase(struct prefix* v, int##bits##_t i);                    \
                                                                               \
    static inline void prefix##_swap_values(                                   \
        struct prefix* v, int##bits##_t a, int##bits##_t b)                    \
    {                                                                          \
        T tmp = v->data[a];                                                    \
        v->data[a] = v->data[b];                                               \
        v->data[b] = tmp;                                                      \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Removes all elements for which the callback function returns     \
     * VEC_ERASE (1). Keeps all elements for which the callback function       \
     * returns VEC_RETAIN (0).                                                 \
     * @param[in] v Pointer to a vector of type VEC(T,B)                       \
     * @param[in] on_element Callback function. Gets called for each element   \
     * once. Return:                                                           \
     *   - VEC_RETAIN (0) to keep the element in the vector.                   \
     *   - VEC_ERASE (1) to remove the element from the vector.                \
     *   - Any negative value to abort iterating and return an error.          \
     * @param[in] user A user defined pointer that gets passed in to the       \
     * callback function. Can be whatever you want.                            \
     * @return Returns 0 if iteration was successful. If the callback function \
     * returns a negative value, then this function will return the same       \
     * negative value. This allows propagating errors from within the callback \
     * function.                                                               \
     */                                                                        \
    int prefix##_retain(                                                       \
        struct prefix* v,                                                      \
        int (*on_element)(T * elem, void* user),                               \
        void* user);                                                           \
                                                                               \
    /*!                                                                        \
     * @brief Reverses the values in a section of the vector.                  \
     * @param[in] v Pointer to a vector of type VEC(T,B)                       \
     * @param[in] start The index of the first element to reverse (inclusive). \
     * @param[in] end The index of the last element to reverse (exclusive).    \
     */                                                                        \
    void prefix##_reverse_range(                                               \
        struct prefix* v, int##bits##_t start, int##bits##_t end);

#define VEC_DEFINE(prefix, T, bits) VEC_DEFINE_FULL(prefix, T, bits, 32, 2)

#define VEC_DEFINE_FULL(prefix, T, bits, MIN_CAPACITY, EXPAND_FACTOR)          \
    int prefix##_realloc(struct prefix** v, int##bits##_t elems)               \
    {                                                                          \
        int            header, data;                                           \
        struct prefix* new_v;                                                  \
                                                                               \
        if (elems == 0)                                                        \
        {                                                                      \
            if (*v)                                                            \
            {                                                                  \
                mem_free(*v);                                                  \
                *v = NULL;                                                     \
            }                                                                  \
            return 0;                                                          \
        }                                                                      \
                                                                               \
        header = offsetof(struct prefix, data);                                \
        data = sizeof(T) * elems;                                              \
        new_v = (struct prefix*)mem_realloc(*v, header + data);                \
        if (new_v == NULL)                                                     \
            return log_oom(header + data, "vec_realloc()");                    \
                                                                               \
        if (*v == NULL)                                                        \
            new_v->count = 0;                                                  \
        if (new_v->count > elems)                                              \
            new_v->count = elems;                                              \
                                                                               \
        new_v->capacity = elems;                                               \
                                                                               \
        *v = new_v;                                                            \
        return 0;                                                              \
    }                                                                          \
    void prefix##_deinit(struct prefix* v)                                     \
    {                                                                          \
        if (v)                                                                 \
            mem_free(v);                                                       \
    }                                                                          \
    void prefix##_compact(struct prefix** v)                                   \
    {                                                                          \
        /* Doesn't matter if this fails -- vector will be in a consistent      \
         * state */                                                            \
        (void)prefix##_realloc(v, vec_count(*v));                              \
    }                                                                          \
    T* prefix##_emplace(struct prefix** v)                                     \
    {                                                                          \
        if (*v == NULL || (*v)->count == (*v)->capacity)                       \
        {                                                                      \
            int old_cap = *v ? (*v)->capacity : 0;                             \
            int new_cap = old_cap ? old_cap * EXPAND_FACTOR : MIN_CAPACITY;    \
            if (prefix##_realloc(v, new_cap) != 0)                             \
                return NULL;                                                   \
            if (old_cap == 0)                                                  \
                (*v)->count = 0;                                               \
            (*v)->capacity = new_cap;                                          \
        }                                                                      \
                                                                               \
        return &(*v)->data[(*v)->count++];                                     \
    }                                                                          \
    T* prefix##_insert_emplace(struct prefix** v, int##bits##_t i)             \
    {                                                                          \
        CLITHER_DEBUG_ASSERT(i >= 0 && i <= (*v ? (*v)->count : 0));           \
                                                                               \
        if (prefix##_emplace(v) == NULL)                                       \
            return NULL;                                                       \
                                                                               \
        memmove(                                                               \
            (*v)->data + i + 1,                                                \
            (*v)->data + i,                                                    \
            ((*v)->count - i - 1) * sizeof(T));                                \
        return &(*(v))->data[i];                                               \
    }                                                                          \
    void prefix##_erase(struct prefix* v, int##bits##_t i)                     \
    {                                                                          \
        --(v->count);                                                          \
        memmove(v->data + i, v->data + i + 1, (v->count - i) * sizeof(T));     \
    }                                                                          \
    int prefix##_retain(                                                       \
        struct prefix* v, int (*on_element)(T * elem, void* user), void* user) \
    {                                                                          \
        int##bits##_t i;                                                       \
        if (v == NULL)                                                         \
            return 0;                                                          \
        for (i = 0; i != v->count; ++i)                                        \
        {                                                                      \
            int result = on_element(&v->data[i], user);                        \
            if (result < 0)                                                    \
                return result;                                                 \
            if (result != VEC_RETAIN)                                          \
            {                                                                  \
                prefix##_erase(v, i);                                          \
                --i;                                                           \
            }                                                                  \
        }                                                                      \
        return 0;                                                              \
    }                                                                          \
    void prefix##_reverse_range(                                               \
        struct prefix* v, int##bits##_t start, int##bits##_t end)              \
    {                                                                          \
        CLITHER_DEBUG_ASSERT(                                                  \
            start >= 0 && start < end && end <= vec_count(v));                 \
        while (start < --end)                                                  \
            prefix##_swap_values(v, start++, end);                             \
    }

#define vec_data(v)     ((v) ? (v)->data : NULL)
#define vec_begin(v)    ((v) ? (v)->data : NULL)
#define vec_count(v)    ((v) ? (v)->count : 0)
#define vec_capacity(v) ((v) ? (v)->capacity : 0)
#define vec_end(v)      ((v) ? (v)->data + (v)->count : NULL)
#define vec_begin_r(v)  ((v) ? (v)->data + (v)->count - 1 : NULL)
#define vec_end_r(v)    ((v) ? (v)->data - 1 : NULL)

/*!
 * @brief Returns the first element of the vector.
 * @warning The returned pointer could be invalidated if any other vector
 * related function is called, as the underlying memory of the vector could be
 * re-allocated. Use the pointer immediately after calling this function.
 * @param[in] v Pointer to a vector of type VEC(T,B)*.
 * @return A pointer to the last element. See warning and use with caution.
 * Vector must not be empty.
 */
#define vec_first(v) (&(v)->data[0])

/*!
 * @brief Returns the first element of the vector.
 * @warning The returned pointer could be invalidated if any other vector
 * related function is called, as the underlying memory of the vector could be
 * re-allocated. Use the pointer immediately after calling this function.
 * @param[in] v Pointer to a vector of type VEC(T,B)*.
 * @return A pointer to the last element. See warning and use with caution.
 * Vector must not be empty.
 */
#define vec_last(v) (&(v)->data[(v)->count - 1])

/*!
 * @brief Returns the nth element of the vector, starting at 0.
 * @warning The returned pointer could be invalidated if any other vector
 * related function is called, as the underlying memory of the vector could be
 * re-allocated. Use the pointer immediately after calling this function.
 * @param[in] v Pointer to a vector of type VEC(T,B)*.
 * @param[in] i Index, ranging from 0 to vec_count()-1
 * @return A pointer to the element. See warning and use with caution.
 * Vector must not be empty.
 */
#define vec_get(v, i) (&(v)->data[(i)])

/*!
 * @brief Returns the nth last element of the vector, starting at count - 1.
 * @warning The returned pointer could be invalidated if any other vector
 * related function is called, as the underlying memory of the vector could be
 * re-allocated. Use the pointer immediately after calling this function.
 * @param[in] v Pointer to a vector of type VEC(T,B)*.
 * @param[in] i Index, ranging from 0 to vec_count()-1
 * @return A pointer to the element. See warning and use with caution.
 * Vector must not be empty.
 */
#define vec_rget(v, i) (&(v)->data[(v)->count - (i) - 1])

/*!
 * @brief Iterates over the elements in a vector.
 *
 *   T* element;
 *   VEC(T,32)* v = some_vector();
 *   vec_for_each(v, element) {
 *       ... do stuff ...
 *   }
 */
#define vec_for_each(v, var) for (var = vec_begin(v); var != vec_end(v); var++)

#define vec_for_each_r(v, var)                                                 \
    for (var = vec_begin_r(v); var != vec_end_r(v); var--)

#define vec_enumerate(v, i, var)                                               \
    for (i = 0; (v) && i != (v)->count && ((var = &(v)->data[i]), 1); ++i)

#if defined(CLITHER_MEM_DEBUGGING)
#    define mem_own_vec(prefix, v)                                             \
        do                                                                     \
        {                                                                      \
            if (v)                                                             \
                mem_own(                                                       \
                    (v),                                                       \
                    offsetof(struct prefix, data) +                            \
                        sizeof((v)->data[0]) * (v)->capacity);                 \
        } while (0)
#    define mem_unown_vec(v) mem_unown(v)
#else
#    define mem_own_vec(v)
#    define mem_unown_vec(v)
#endif
