#pragma once

#include "clither/log.h" /* log_oom */
#include "clither/mem.h" /* mem_realloc, mem_free */
#include <stddef.h>
#include <stdint.h> /* NULL */
#include <string.h> /* memmove */

enum bset_status
{
    BSET_OOM = -1,
    BSET_EXISTS = 0,
    BSET_NEW = 1
};

#define BSET_RETAIN 0
#define BSET_ERASE  1

#define BSET_DECLARE(prefix, K, bits)                                          \
    /* Default key storage implementation */                                   \
    struct prefix                                                              \
    {                                                                          \
        int##bits##_t count, capacity;                                         \
        K             keys[1];                                                 \
    };                                                                         \
    BSET_DECLARE_FULL(prefix, K, bits)

#define BSET_DECLARE_FULL(prefix, K, bits)                                     \
    /*! @brief Call this before using any other functions. */                  \
    static void prefix##_init(struct prefix** bset)                            \
    {                                                                          \
        *bset = NULL;                                                          \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Destroys an existing bset and frees all memory allocated by      \
     * inserted elements.                                                      \
     */                                                                        \
    void prefix##_deinit(struct prefix* bset);                                 \
                                                                               \
    static void prefix##_clear(struct prefix* bset)                            \
    {                                                                          \
        if (bset)                                                              \
            bset->count = 0;                                                   \
    }                                                                          \
                                                                               \
    void prefix##_compact(struct prefix** bset);                               \
                                                                               \
    /*!                                                                        \
     * @brief Inserts a key.                                                   \
     * @param[in] bset Pointer to an initialized bset.                         \
     * @param[in] key The key to insert.                                       \
     * @return Returns BSET_OOM if allocation fails. Returns BSET_NEW if the   \
     * key did not exist. Returns BSET_EXISTS if the key existed.              \
     */                                                                        \
    enum bset_status prefix##_insert(struct prefix** bset, K key);             \
                                                                               \
    /*!                                                                        \
     * @brief Removes a key from the bset.                                     \
     * @param[in] bset Pointer to an initialized bset.                         \
     * @param[in] key The key to erase.                                        \
     * @return If the key exists, returns 1. If the key does not exist,        \
     * returns 0.                                                              \
     */                                                                        \
    int prefix##_erase(struct prefix* bset, K key);                            \
                                                                               \
    /*!                                                                        \
     * @brief Removes all keys for which the callback function returns         \
     * BSET_ERASE (1). Keeps all keys for which the callback function          \
     * returns BSET_RETAIN (0).                                                \
     * @param[in] bset Pointer to an initialized bset.                         \
     * @param[in] on_element Callback function. Gets called for each element   \
     * once. Return:                                                           \
     *   - BSET_RETAIN (0) to keep the element in the bset.                    \
     *   - BSET_ERASE (1) to remove the element from the bset.                 \
     *   - Any negative value to abort iterating and return an error.          \
     * @param[in] user A user defined pointer that gets passed in to the       \
     * callback function. Can be whatever you want.                            \
     * @return Returns 0 if iteration was successful. If the callback function \
     * returns a negative value, then this function will return the same       \
     * negative value. This allows propagating errors from within the callback \
     * function.                                                               \
     */                                                                        \
    int prefix##_retain(                                                       \
        struct prefix* v, int (*on_element)(K key, void* user), void* user);   \
                                                                               \
    /*!                                                                        \
     * @brief Finds the specified key.                                         \
     * @param[in] bset Pointer to an initialized bset.                         \
     * @param[in] key The key to find.                                         \
     * @return Returns 1 if the key exists, or 0 if the key doesn't exist.     \
     */                                                                        \
    int prefix##_find(struct prefix* bset, K key);

#define BSET_DEFINE(prefix, K, bits)                                           \
    /* Default key storage implementation */                                   \
    static void prefix##_kvs_deinit(struct prefix* bset)                       \
    {                                                                          \
        if (bset)                                                              \
            mem_free(bset);                                                    \
    }                                                                          \
    static int prefix##_kvs_realloc(                                           \
        struct prefix** bset, int##bits##_t new_capacity)                      \
    {                                                                          \
        int##bits##_t  header, data;                                           \
        struct prefix* new_bset;                                               \
                                                                               \
        if (new_capacity == 0)                                                 \
        {                                                                      \
            if (*bset)                                                         \
            {                                                                  \
                prefix##_kvs_deinit(*bset);                                    \
                *bset = NULL;                                                  \
            }                                                                  \
            return 0;                                                          \
        }                                                                      \
                                                                               \
        header = offsetof(struct prefix, keys);                                \
        data = sizeof(K) * new_capacity;                                       \
        new_bset = (struct prefix*)mem_realloc(*bset, header + data);          \
        if (new_bset == NULL)                                                  \
            return log_oom(header + data, "bset_bset_insert()");               \
        if (*bset == NULL)                                                     \
            new_bset->count = 0;                                               \
        new_bset->capacity = new_capacity;                                     \
        *bset = new_bset;                                                      \
        return 0;                                                              \
    }                                                                          \
    static int prefix##_kvs_insert(                                            \
        struct prefix** bset, int##bits##_t idx, K key)                        \
    {                                                                          \
        if (!*bset || (*bset)->count == (*bset)->capacity)                     \
        {                                                                      \
            int##bits##_t new_capacity = (*bset) ? (*bset)->capacity * 2 : 32; \
            if (prefix##_kvs_realloc(bset, new_capacity) != 0)                 \
                return -1;                                                     \
        }                                                                      \
                                                                               \
        (*bset)->count++;                                                      \
        memmove(                                                               \
            (*bset)->keys + idx + 1,                                           \
            (*bset)->keys + idx,                                               \
            ((*bset)->count - idx - 1) * sizeof(K));                           \
                                                                               \
        (*bset)->keys[idx] = key;                                              \
        return 0;                                                              \
    }                                                                          \
    static void prefix##_kvs_erase(struct prefix* bset, int##bits##_t idx)     \
    {                                                                          \
        --bset->count;                                                         \
        memmove(                                                               \
            bset->keys + idx,                                                  \
            bset->keys + idx + 1,                                              \
            (bset->count - idx) * sizeof(K));                                  \
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
        const struct prefix* bset, int##bits##_t idx)                          \
    {                                                                          \
        return bset->keys[idx];                                                \
    }                                                                          \
    BSET_DEFINE_FULL(                                                          \
        prefix,                                                                \
        K,                                                                     \
        bits,                                                                  \
        prefix##_kvs_deinit,                                                   \
        prefix##_kvs_realloc,                                                  \
        prefix##_kvs_insert,                                                   \
        prefix##_kvs_erase,                                                    \
        prefix##_kvs_less_than,                                                \
        prefix##_kvs_equal,                                                    \
        prefix##_kvs_get_key)

#define BSET_DEFINE_FULL(                                                      \
    prefix,                                                                    \
    K,                                                                         \
    bits,                                                                      \
    kvs_deinit,                                                                \
    kvs_realloc,                                                               \
    kvs_insert,                                                                \
    kvs_erase,                                                                 \
    kvs_less_than,                                                             \
    kvs_equal,                                                                 \
    kvs_get_key)                                                               \
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
     *    the returned index will be bset_count().                             \
     */                                                                        \
    static int##bits##_t prefix##_lower_bound(                                 \
        const struct prefix* bset, K key)                                      \
    {                                                                          \
        int##bits##_t len, half, middle, found;                                \
                                                                               \
        found = 0; /* Begin search at start of array */                        \
        len = bset_count(bset);                                                \
                                                                               \
        while (len > 0)                                                        \
        {                                                                      \
            half = len >> 1;                                                   \
            middle = found + half;                                             \
            if (kvs_less_than(kvs_get_key(bset, middle), key))                 \
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
    void prefix##_deinit(struct prefix* bset)                                  \
    {                                                                          \
        /* These don't do anything, except act as a poor-man's type-check for  \
         * the various key-value storage functions. */                         \
        void (*deinit)(struct prefix*) = kvs_deinit;                           \
        int (*insert)(struct prefix**, int##bits##_t, K) = kvs_insert;         \
        void (*erase)(struct prefix*, int##bits##_t) = kvs_erase;              \
        int (*less_than)(K, K) = kvs_less_than;                                \
        int (*equal)(K, K) = kvs_equal;                                        \
        K(*get_key)                                                            \
        (const struct prefix*, int##bits##_t) = kvs_get_key;                   \
        (void)deinit;                                                          \
        (void)insert;                                                          \
        (void)erase;                                                           \
        (void)less_than;                                                       \
        (void)equal;                                                           \
        (void)get_key;                                                         \
                                                                               \
        kvs_deinit(bset);                                                      \
    }                                                                          \
                                                                               \
    void prefix##_compact(struct prefix** bset)                                \
    {                                                                          \
        kvs_realloc(bset, bset_count(*bset));                                  \
    }                                                                          \
                                                                               \
    enum bset_status prefix##_insert(struct prefix** bset, K key)              \
    {                                                                          \
        int##bits##_t idx = prefix##_lower_bound(*bset, key);                  \
        if (idx < bset_count(*bset) &&                                         \
            kvs_equal(key, kvs_get_key(*bset, idx)))                           \
            return BSET_EXISTS;                                                \
                                                                               \
        if (kvs_insert(bset, idx, key) != 0)                                   \
            return BSET_OOM;                                                   \
        return BSET_NEW;                                                       \
    }                                                                          \
                                                                               \
    int prefix##_erase(struct prefix* bset, K key)                             \
    {                                                                          \
        int##bits##_t idx = prefix##_lower_bound(bset, key);                   \
        if (idx < bset_count(bset) && kvs_equal(key, kvs_get_key(bset, idx)))  \
        {                                                                      \
            kvs_erase(bset, idx);                                              \
            return 1;                                                          \
        }                                                                      \
        return 0;                                                              \
    }                                                                          \
                                                                               \
    int prefix##_retain(                                                       \
        struct prefix* bset, int (*on_element)(K key, void* user), void* user) \
    {                                                                          \
        int##bits##_t i;                                                       \
        if (bset == NULL)                                                      \
            return 0;                                                          \
        for (i = 0; i != bset->count; ++i)                                     \
        {                                                                      \
            int result = on_element(kvs_get_key(bset, i), user);               \
            if (result < 0)                                                    \
                return result;                                                 \
            if (result != BSET_RETAIN)                                         \
            {                                                                  \
                kvs_erase(bset, i);                                            \
                --i;                                                           \
            }                                                                  \
        }                                                                      \
        return 0;                                                              \
    }                                                                          \
                                                                               \
    int prefix##_find(struct prefix* bset, K key)                              \
    {                                                                          \
        int##bits##_t idx = prefix##_lower_bound(bset, key);                   \
        return idx < bset_count(bset) &&                                       \
               kvs_equal(key, kvs_get_key(bset, idx));                         \
    }

#define bset_count(bset)    ((bset) ? (bset)->count : 0)
#define bset_capacity(bset) ((bset) ? (bset)->capacity : 0)

#define bset_for_each(bset, idx, key)                                          \
    for (idx = 0; idx != bset_count(bset) && (key = (bset)->keys[idx], 1);     \
         ++idx)

#define bset_for_each_full(bset, idx, key, value, kvs_get_key)                 \
    for (idx = 0;                                                              \
         idx != bset_count(bset) && (bset = kvs_get_key(bset, idx), 1);        \
         ++idx)
