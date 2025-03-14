#pragma once

#include "clither/log.h" /* log_oom */
#include "clither/mem.h" /* mem_realloc, mem_free */
#include <stddef.h>
#include <stdint.h> /* NULL */
#include <string.h> /* memmove */

enum bmap_status
{
    BMAP_OOM = -1,
    BMAP_EXISTS = 0,
    BMAP_NEW = 1
};

#define BMAP_RETAIN 0
#define BMAP_ERASE  1

#define BMAP_DECLARE(prefix, K, V, bits)                                       \
    /* Default key-value storage malloc()'s two arrays */                      \
    struct prefix                                                              \
    {                                                                          \
        int##bits##_t count, capacity;                                         \
        K*            keys;                                                    \
        V             values[1];                                               \
    };                                                                         \
    BMAP_DECLARE_FULL(prefix, K, V, bits)

#define BMAP_DECLARE_FULL(prefix, K, V, bits)                                  \
    /*! @brief Call this before using any other functions. */                  \
    static void prefix##_init(struct prefix** bmap)                            \
    {                                                                          \
        *bmap = NULL;                                                          \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Destroys an existing bmap and frees all memory allocated by      \
     * inserted elements.                                                      \
     */                                                                        \
    void prefix##_deinit(struct prefix* bmap);                                 \
                                                                               \
    static void prefix##_clear(struct prefix* bmap)                            \
    {                                                                          \
        if (bmap)                                                              \
            bmap->count = 0;                                                   \
    }                                                                          \
                                                                               \
    void prefix##_compact(struct prefix** bmap);                               \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and allocates space   \
     * for a new value.                                                        \
     * @param[in] bmap Pointer to an initialized bmap.                         \
     * @param[in] key The key to insert.                                       \
     * @param[out] value Address of the newly allocated value is written to    \
     * this parameter.                                                         \
     * @return                                                                 \
     *   - BMAP_OOM if allocation fails.                                       \
     *   - BMAP_EXISTS if the key exists. "value" remains untouched.           \
     *   - BMAP_NEW if the key did not exist and was inserted. "value" will    \
     *     point to a newly allocated value.                                   \
     */                                                                        \
    enum bmap_status prefix##_emplace_new(                                     \
        struct prefix** bmap, K key, V** value);                               \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and allocates space   \
     * for a new value. If the key exists, returns the existing value.         \
     * @param[in] bmap Pointer to an initialized bmap.                         \
     * @param[in] key The key to insert or find.                               \
     * @param[out] value Address of the existing value, or address of the      \
     * newly allocated value is written to this parameter.                     \
     * @return Returns BMAP_OOM if allocation fails. Returns BMAP_EXISTS if    \
     * the key exists and value will point to the existing value. Returns      \
     * BMAP_NEW if the key did not exist and was inserted and value will       \
     * point to a newly allocated value.                                       \
     */                                                                        \
    enum bmap_status prefix##_emplace_or_get(                                  \
        struct prefix** bmap, K key, V** value);                               \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and value.            \
     * @param[in] bmap Pointer to an initialized bmap.                         \
     * @param[in] key The key to insert.                                       \
     * @param[in] value The value to insert, if the key does not exist.        \
     * @return Returns 0 if the key+value was inserted. Returns -1 if the key  \
     * exists, or if allocation failed.                                        \
     */                                                                        \
    static enum bmap_status prefix##_insert_new(                               \
        struct prefix** bmap, K key, V value)                                  \
    {                                                                          \
        V*               ins_value;                                            \
        enum bmap_status status = prefix##_emplace_new(bmap, key, &ins_value); \
        if (status == BMAP_NEW)                                                \
            *ins_value = value;                                                \
        return status;                                                         \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and value. If the key \
     * exists, then the existing value is overwritten with the new value.      \
     * @param[in] bmap Pointer to an initialized bmap.                         \
     * @param[in] key The key to insert or find.                               \
     * @param[in] value The value to insert or update.                         \
     * @return Returns -1 if allocation failed. Returns 0 otherwise.           \
     */                                                                        \
    static enum bmap_status prefix##_insert_update(                            \
        struct prefix** bmap, K key, V value)                                  \
    {                                                                          \
        V*               ins_value;                                            \
        enum bmap_status status =                                              \
            prefix##_emplace_or_get(bmap, key, &ins_value);                    \
        if (status != BMAP_OOM)                                                \
            *ins_value = value;                                                \
        return status;                                                         \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Removes the key and its associated value from the bmap.          \
     * @param[in] bmap Pointer to an initialized bmap.                         \
     * @param[in] key The key to erase.                                        \
     * @return If the key exists, returns 1. If the key does not exist,        \
     * returns 0.                                                              \
     */                                                                        \
    int prefix##_erase(struct prefix* bmap, K key);                            \
                                                                               \
    /*!                                                                        \
     * @brief Removes all elements for which the callback function returns     \
     * BMAP_ERASE (1). Keeps all elements for which the callback function      \
     * returns BMAP_RETAIN (0).                                                \
     * @param[in] bmap Pointer to an initialized bmap.                         \
     * @param[in] on_element Callback function. Gets called for each element   \
     * once. Return:                                                           \
     *   - BMAP_RETAIN (0) to keep the element in the bmap.                    \
     *   - BMAP_ERASE (1) to remove the element from the bmap.                 \
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
        int (*on_element)(K key, V * value, void* user),                       \
        void* user);                                                           \
                                                                               \
    /*!                                                                        \
     * @brief Finds the value associated with the specified key.               \
     * @param[in] bmap Pointer to an initialized bmap.                         \
     * @param[in] key The key to find.                                         \
     * @return If the key exists, a pointer to the value is returned. It is    \
     * valid to read/write to the value until the next bmap operation. If the  \
     * key does not exist, NULL is returned.                                   \
     */                                                                        \
    V* prefix##_find(struct prefix* bmap, K key);

#define BMAP_DEFINE(prefix, K, V, bits)                                        \
    /* Default key-value storage implementation */                             \
    static void prefix##_kvs_deinit(struct prefix* bmap)                       \
    {                                                                          \
        if (bmap)                                                              \
        {                                                                      \
            mem_free(bmap->keys);                                              \
            mem_free(bmap);                                                    \
        }                                                                      \
    }                                                                          \
    static int prefix##_kvs_realloc(                                           \
        struct prefix** bmap, int##bits##_t new_capacity)                      \
    {                                                                          \
        int##bits##_t  header, data;                                           \
        struct prefix* new_bmap;                                               \
        K*             new_keys;                                               \
                                                                               \
        if (new_capacity == 0)                                                 \
        {                                                                      \
            if (*bmap)                                                         \
            {                                                                  \
                prefix##_kvs_deinit(*bmap);                                    \
                *bmap = NULL;                                                  \
            }                                                                  \
            return 0;                                                          \
        }                                                                      \
                                                                               \
        header = offsetof(struct prefix, values);                              \
        data = sizeof(V) * new_capacity;                                       \
        new_bmap = (struct prefix*)mem_realloc(*bmap, header + data);          \
        if (new_bmap == NULL)                                                  \
            return log_oom(header + data, "bmap_bmap_emplace()");              \
        if (*bmap == NULL)                                                     \
        {                                                                      \
            new_bmap->count = 0;                                               \
            new_bmap->keys = NULL;                                             \
        }                                                                      \
        new_bmap->capacity = new_capacity;                                     \
                                                                               \
        new_keys = (K*)mem_realloc(new_bmap->keys, sizeof(K) * new_capacity);  \
        if (new_keys == NULL)                                                  \
            return log_oom(sizeof(K) * new_capacity, "bmap_bmap_emplace()");   \
                                                                               \
        new_bmap->keys = new_keys;                                             \
        *bmap = new_bmap;                                                      \
        return 0;                                                              \
    }                                                                          \
    static V* prefix##_kvs_emplace(                                            \
        struct prefix** bmap, int##bits##_t idx, K key)                        \
    {                                                                          \
        if (!*bmap || (*bmap)->count == (*bmap)->capacity)                     \
        {                                                                      \
            int##bits##_t new_capacity = (*bmap) ? (*bmap)->capacity * 2 : 32; \
            if (prefix##_kvs_realloc(bmap, new_capacity) != 0)                 \
                return NULL;                                                   \
        }                                                                      \
                                                                               \
        (*bmap)->count++;                                                      \
        memmove(                                                               \
            (*bmap)->keys + idx + 1,                                           \
            (*bmap)->keys + idx,                                               \
            ((*bmap)->count - idx - 1) * sizeof(K));                           \
        memmove(                                                               \
            (*bmap)->values + idx + 1,                                         \
            (*bmap)->values + idx,                                             \
            ((*bmap)->count - idx - 1) * sizeof(V));                           \
                                                                               \
        (*bmap)->keys[idx] = key;                                              \
        return &(*bmap)->values[idx];                                          \
    }                                                                          \
    static void prefix##_kvs_erase(struct prefix* bmap, int##bits##_t idx)     \
    {                                                                          \
        --bmap->count;                                                         \
        memmove(                                                               \
            bmap->keys + idx,                                                  \
            bmap->keys + idx + 1,                                              \
            (bmap->count - idx) * sizeof(K));                                  \
        memmove(                                                               \
            bmap->values + idx,                                                \
            bmap->values + idx + 1,                                            \
            (bmap->count - idx) * sizeof(V));                                  \
    }                                                                          \
    static int prefix##_kvs_less_than(K k1, K k2)                              \
    {                                                                          \
        return k1 < k2;                                                        \
    }                                                                          \
    static int prefix##_kvs_equal(K k1, K k2)                                  \
    {                                                                          \
        return k1 == k2;                                                       \
    }                                                                          \
    static K prefix##_kvs_get_key(                                             \
        const struct prefix* bmap, int##bits##_t idx)                          \
    {                                                                          \
        return bmap->keys[idx];                                                \
    }                                                                          \
    static V* prefix##_kvs_get_value(struct prefix* bmap, int##bits##_t idx)   \
    {                                                                          \
        return &bmap->values[idx];                                             \
    }                                                                          \
    static void prefix##_kvs_set_value(                                        \
        struct prefix* bmap, int##bits##_t idx, const V* value)                \
    {                                                                          \
        bmap->values[idx] = *value;                                            \
    }                                                                          \
    BMAP_DEFINE_FULL(                                                          \
        prefix,                                                                \
        K,                                                                     \
        V,                                                                     \
        bits,                                                                  \
        prefix##_kvs_deinit,                                                   \
        prefix##_kvs_realloc,                                                  \
        prefix##_kvs_emplace,                                                  \
        prefix##_kvs_erase,                                                    \
        prefix##_kvs_less_than,                                                \
        prefix##_kvs_equal,                                                    \
        prefix##_kvs_get_key,                                                  \
        prefix##_kvs_get_value)

#define BMAP_DEFINE_FULL(                                                      \
    prefix,                                                                    \
    K,                                                                         \
    V,                                                                         \
    bits,                                                                      \
    kvs_deinit,                                                                \
    kvs_realloc,                                                               \
    kvs_emplace,                                                               \
    kvs_erase,                                                                 \
    kvs_less_than,                                                             \
    kvs_equal,                                                                 \
    kvs_get_key,                                                               \
    kvs_get_value)                                                             \
                                                                               \
    /*                                                                         \
     * algorithm based on GNU GCC stdlibc++'s lower_bound function, line 2121  \
     * in stl_algo.h                                                           \
     * gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-4.3/a02014.html   \
     *                                                                         \
     * 1) If the key exists, then the index of that key is returned.           \
     * 2) If the key does not exist, then the index of the first valid key     \
     *    who's value is less than the key being searched for is returned.     \
     * 3) If there is no key who's value is less than the searched-for key,    \
     *    the returned index will be bmap_count().                             \
     */                                                                        \
    static int##bits##_t prefix##_lower_bound(                                 \
        const struct prefix* bmap, K key)                                      \
    {                                                                          \
        int##bits##_t len, half, middle, found;                                \
                                                                               \
        found = 0; /* Begin search at start of array */                        \
        len = bmap_count(bmap);                                                \
                                                                               \
        while (len > 0)                                                        \
        {                                                                      \
            half = len >> 1;                                                   \
            middle = found + half;                                             \
            if (kvs_less_than(kvs_get_key(bmap, middle), key))                 \
            {                                                                  \
                found = middle;                                                \
                ++found;                                                       \
                len = len - half - 1;                                          \
            }                                                                  \
            else                                                               \
                len = half;                                                    \
        }                                                                      \
        return found;                                                          \
    }                                                                          \
                                                                               \
    void prefix##_deinit(struct prefix* bmap)                                  \
    {                                                                          \
        /* These don't do anything, except act as a poor-man's type-check for  \
         * the various key-value storage functions. */                         \
        void (*deinit)(struct prefix*) = kvs_deinit;                           \
        V* (*emplace)(struct prefix**, int##bits##_t, K) = kvs_emplace;        \
        void (*erase)(struct prefix*, int##bits##_t) = kvs_erase;              \
        int (*less_than)(K, K) = kvs_less_than;                                \
        int (*equal)(K, K) = kvs_equal;                                        \
        K(*get_key)                                                            \
        (const struct prefix*, int##bits##_t) = kvs_get_key;                   \
        V* (*get_value)(struct prefix*, int##bits##_t) = kvs_get_value;        \
        (void)deinit;                                                          \
        (void)emplace;                                                         \
        (void)erase;                                                           \
        (void)less_than;                                                       \
        (void)equal;                                                           \
        (void)get_key;                                                         \
        (void)get_value;                                                       \
                                                                               \
        kvs_deinit(bmap);                                                      \
    }                                                                          \
                                                                               \
    void prefix##_compact(struct prefix** bmap)                                \
    {                                                                          \
        kvs_realloc(bmap, bmap_count(*bmap));                                  \
    }                                                                          \
                                                                               \
    enum bmap_status prefix##_emplace_new(                                     \
        struct prefix** bmap, K key, V** value)                                \
    {                                                                          \
        int##bits##_t idx = prefix##_lower_bound(*bmap, key);                  \
        if (idx < bmap_count(*bmap) &&                                         \
            kvs_equal(key, kvs_get_key(*bmap, idx)))                           \
            return BMAP_EXISTS;                                                \
                                                                               \
        *value = kvs_emplace(bmap, idx, key);                                  \
        if (*value == NULL)                                                    \
            return BMAP_OOM;                                                   \
        return BMAP_NEW;                                                       \
    }                                                                          \
                                                                               \
    enum bmap_status prefix##_emplace_or_get(                                  \
        struct prefix** bmap, K key, V** value)                                \
    {                                                                          \
        V*            emplaced;                                                \
        int##bits##_t idx = prefix##_lower_bound(*bmap, key);                  \
        if (idx < bmap_count(*bmap) &&                                         \
            kvs_equal(key, kvs_get_key(*bmap, idx)))                           \
        {                                                                      \
            *value = kvs_get_value(*bmap, idx);                                \
            return BMAP_EXISTS;                                                \
        }                                                                      \
                                                                               \
        emplaced = kvs_emplace(bmap, idx, key);                                \
        if (emplaced == NULL)                                                  \
            return BMAP_OOM;                                                   \
        *value = emplaced;                                                     \
        return BMAP_NEW;                                                       \
    }                                                                          \
                                                                               \
    int prefix##_erase(struct prefix* bmap, K key)                             \
    {                                                                          \
        int##bits##_t idx = prefix##_lower_bound(bmap, key);                   \
        if (idx < bmap_count(bmap) && kvs_equal(key, kvs_get_key(bmap, idx)))  \
        {                                                                      \
            kvs_erase(bmap, idx);                                              \
            return 1;                                                          \
        }                                                                      \
        return 0;                                                              \
    }                                                                          \
                                                                               \
    int prefix##_retain(                                                       \
        struct prefix* bmap,                                                   \
        int (*on_element)(K key, V * value, void* user),                       \
        void* user)                                                            \
    {                                                                          \
        int##bits##_t i;                                                       \
        if (bmap == NULL)                                                      \
            return 0;                                                          \
        for (i = 0; i != bmap->count; ++i)                                     \
        {                                                                      \
            int result = on_element(                                           \
                kvs_get_key(bmap, i), kvs_get_value(bmap, i), user);           \
            if (result < 0)                                                    \
                return result;                                                 \
            if (result != BMAP_RETAIN)                                         \
            {                                                                  \
                kvs_erase(bmap, i);                                            \
                --i;                                                           \
            }                                                                  \
        }                                                                      \
        return 0;                                                              \
    }                                                                          \
                                                                               \
    V* prefix##_find(struct prefix* bmap, K key)                               \
    {                                                                          \
        int##bits##_t idx = prefix##_lower_bound(bmap, key);                   \
        if (idx == bmap_count(bmap) ||                                         \
            !kvs_equal(key, kvs_get_key(bmap, idx)))                           \
            return NULL;                                                       \
        return kvs_get_value(bmap, idx);                                       \
    }

#define bmap_count(bmap)    ((bmap) ? (bmap)->count : 0)
#define bmap_capacity(bmap) ((bmap) ? (bmap)->capacity : 0)

#define bmap_for_each(bmap, idx, key, value)                                   \
    for (idx = 0; idx != bmap_count(bmap) && (key = (bmap)->keys[idx], 1) &&   \
                  (value = &(bmap)->values[idx], 1);                           \
         ++idx)

#define bmap_for_each_full(bmap, idx, key, value, kvs_get_key, kvs_get_value)  \
    for (idx = 0;                                                              \
         idx != bmap_count(bmap) && (bmap = kvs_get_key(bmap, idx), 1) &&      \
         (value = kvs_get_value(bmap, idx), 1);                                \
         ++idx)
