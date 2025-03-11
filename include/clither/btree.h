#pragma once

#include "clither/log.h" /* log_oom */
#include "clither/mem.h" /* mem_realloc, mem_free */
#include <stddef.h>
#include <stdint.h> /* NULL */
#include <string.h> /* memmove */

enum btree_status
{
    BTREE_OOM = -1,
    BTREE_EXISTS = 0,
    BTREE_NEW = 1
};

#define BTREE_DECLARE(prefix, K, V, bits)                                      \
    /* Default key-value storage malloc()'s two arrays */                      \
    struct prefix                                                              \
    {                                                                          \
        int##bits##_t count, capacity;                                         \
        K*            keys;                                                    \
        V             values[1];                                               \
    };                                                                         \
    BTREE_DECLARE_FULL(prefix, K, V, bits)

#define BTREE_DECLARE_FULL(prefix, K, V, bits)                                 \
    /*! @brief Call this before using any other functions. */                  \
    static inline void prefix##_init(struct prefix** btree)                    \
    {                                                                          \
        *btree = NULL;                                                         \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Destroys an existing btree and frees all memory allocated by     \
     * inserted elements.                                                      \
     */                                                                        \
    void prefix##_deinit(struct prefix* btree);                                \
                                                                               \
    static inline void prefix##_clear(struct prefix* btree)                    \
    {                                                                          \
        if (btree)                                                             \
            btree->count = 0;                                                  \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and allocates space   \
     * for a new value.                                                        \
     * @param[in] btree Pointer to an initialized btree.                       \
     * @param[in] key The key to insert.                                       \
     * @return A pointer to the allocated value. If the key already exists, or \
     * if allocation fails, NULL is returned.                                  \
     */                                                                        \
    V* prefix##_emplace_new(struct prefix** btree, K key);                     \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and allocates space   \
     * for a new value. If the key exists, returns the existing value.         \
     * @param[in] btree Pointer to an initialized btree.                       \
     * @param[in] key The key to insert or find.                               \
     * @param[out] value Address of the existing value, or address of the      \
     * newly allocated value is written to this parameter.                     \
     * @return Returns BTREE_OOM if allocation fails. Returns BTREE_EXISTS if  \
     * the key exists and value will point to the existing value. Returns      \
     * BTREE_NEW if the key did not exist and was inserted and value will      \
     * point to a newly allocated value.                                       \
     */                                                                        \
    enum btree_status prefix##_emplace_or_get(                                 \
        struct prefix** btree, K key, V** value);                              \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and value.            \
     * @param[in] btree Pointer to an initialized btree.                       \
     * @param[in] key The key to insert.                                       \
     * @param[in] value The value to insert, if the key does not exist.        \
     * @return Returns 0 if the key+value was inserted. Returns -f if the key  \
     * exists, or if allocation failed.                                        \
     */                                                                        \
    static inline int prefix##_insert_new(                                     \
        struct prefix** btree, K key, V value)                                 \
    {                                                                          \
        V* emplaced = prefix##_emplace_new(btree, key);                        \
        if (emplaced == NULL)                                                  \
            return -1;                                                         \
        *emplaced = value;                                                     \
        return 0;                                                              \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and value. If the key \
     * exists, then the existing value is overwritten with the new value.      \
     * @param[in] btree Pointer to an initialized btree.                       \
     * @param[in] key The key to insert or find.                               \
     * @param[in] value The value to insert or update.                         \
     * @return Returns BTREE_OOM if allocation failed. Returns BTREE_EXISTS if \
     * the key existed and the value was updated. Returns BTREE_NEW if the key \
     * did not exist and the value was inserted.                               \
     */                                                                        \
    static inline int prefix##_insert_update(                                  \
        struct prefix** btree, K key, V value)                                 \
    {                                                                          \
        V* ins_value;                                                          \
        switch (prefix##_emplace_or_get(btree, key, &ins_value))               \
        {                                                                      \
            case BTREE_OOM: return -1;                                         \
            case BTREE_EXISTS:                                                 \
            case BTREE_NEW: *ins_value = value; break;                         \
        }                                                                      \
        return 0;                                                              \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Removes the key and its associated value from the btree.         \
     * @param[in] btree Pointer to an initialized btree.                       \
     * @param[in] key The key to erase.                                        \
     * @return If the key exists, returns a pointer to the value that was      \
     * removed. It is valid to read/write from this pointer until the next     \
     * btree operation. If the key does not exist, returns NULL.               \
     */                                                                        \
    void prefix##_erase(struct prefix* btree, K key);                          \
                                                                               \
    /*!                                                                        \
     * @brief Finds the value associated with the specified key.               \
     * @param[in] btree Pointer to an initialized btree.                       \
     * @param[in] key The key to find.                                         \
     * @return If the key exists, a pointer to the value is returned. It is    \
     * valid to read/write to the value until the next btree operation. If the \
     * key does not exist, NULL is returned.                                   \
     */                                                                        \
    V* prefix##_find(struct prefix* btree, K key);

#define BTREE_DEFINE(prefix, K, V, bits)                                       \
    /* Default key-value storage implementation */                             \
    void prefix##_kvs_deinit(struct prefix* btree)                             \
    {                                                                          \
        if (btree)                                                             \
        {                                                                      \
            mem_free(btree->keys);                                             \
            mem_free(btree);                                                   \
        }                                                                      \
    }                                                                          \
    static V* prefix##_kvs_emplace(                                            \
        struct prefix** btree, int##bits##_t idx, K key)                       \
    {                                                                          \
        if (!*btree || (*btree)->count == (*btree)->capacity)                  \
        {                                                                      \
            int##bits##_t  capacity, header, data;                             \
            struct prefix* new_btree;                                          \
            K*             new_keys;                                           \
                                                                               \
            capacity = (*btree) ? (*btree)->capacity * 2 : 32;                 \
            header = offsetof(struct prefix, values);                          \
            data = sizeof(V) * capacity;                                       \
            new_btree = (struct prefix*)mem_realloc(*btree, header + data);    \
            if (new_btree == NULL)                                             \
            {                                                                  \
                log_oom(header + data, "btree_btree_emplace()");               \
                return NULL;                                                   \
            }                                                                  \
            if (*btree == NULL)                                                \
            {                                                                  \
                new_btree->count = 0;                                          \
                new_btree->keys = NULL;                                        \
            }                                                                  \
            new_btree->capacity = capacity;                                    \
                                                                               \
            new_keys = (K*)mem_realloc(new_btree->keys, sizeof(K) * capacity); \
            if (new_keys == NULL)                                              \
            {                                                                  \
                log_oom(sizeof(K) * capacity, "btree_btree_emplace()");        \
                return NULL;                                                   \
            }                                                                  \
                                                                               \
            new_btree->keys = new_keys;                                        \
            *btree = new_btree;                                                \
        }                                                                      \
                                                                               \
        memmove(                                                               \
            (*btree)->keys + idx + 1,                                          \
            (*btree)->keys + idx,                                              \
            ((*btree)->count - idx - 1) * sizeof(K));                          \
        memmove(                                                               \
            (*btree)->values + idx + 1,                                        \
            (*btree)->values + idx,                                            \
            ((*btree)->count - idx - 1) * sizeof(V));                          \
                                                                               \
        (*btree)->keys[idx] = key;                                             \
        (*btree)->count++;                                                     \
                                                                               \
        return &(*btree)->values[idx];                                         \
    }                                                                          \
    static void prefix##_kvs_erase(struct prefix* btree, int##bits##_t idx)    \
    {                                                                          \
        --btree->count;                                                        \
        memmove(                                                               \
            btree->keys + idx,                                                 \
            btree->keys + idx + 1,                                             \
            (btree->count - idx) * sizeof(K));                                 \
        memmove(                                                               \
            btree->values + idx,                                               \
            btree->values + idx + 1,                                           \
            (btree->count - idx) * sizeof(V));                                 \
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
        const struct prefix* btree, int##bits##_t idx)                         \
    {                                                                          \
        return btree->keys[idx];                                               \
    }                                                                          \
    static V* prefix##_kvs_get_value(struct prefix* btree, int##bits##_t idx)  \
    {                                                                          \
        return &btree->values[idx];                                            \
    }                                                                          \
    static void prefix##_kvs_set_value(                                        \
        struct prefix* btree, int##bits##_t idx, const V* value)               \
    {                                                                          \
        btree->values[idx] = *value;                                           \
    }                                                                          \
    BTREE_DEFINE_FULL(                                                         \
        prefix,                                                                \
        K,                                                                     \
        V,                                                                     \
        bits,                                                                  \
        prefix##_kvs_deinit,                                                   \
        prefix##_kvs_emplace,                                                  \
        prefix##_kvs_erase,                                                    \
        prefix##_kvs_less_than,                                                \
        prefix##_kvs_equal,                                                    \
        prefix##_kvs_get_key,                                                  \
        prefix##_kvs_get_value)

#define BTREE_DEFINE_FULL(                                                     \
    prefix,                                                                    \
    K,                                                                         \
    V,                                                                         \
    bits,                                                                      \
    kvs_deinit,                                                                \
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
     *    the returned index will be btree_count().                            \
     */                                                                        \
    static int##bits##_t prefix##_lower_bound(                                 \
        const struct prefix* btree, K key)                                     \
    {                                                                          \
        int##bits##_t len, half, middle, found;                                \
                                                                               \
        found = 0; /* Begin search at start of array */                        \
        len = btree_count(btree);                                              \
                                                                               \
        while (len > 0)                                                        \
        {                                                                      \
            half = len >> 1;                                                   \
            middle = found + half;                                             \
            if (kvs_less_than(                                                 \
                    kvs_get_key(btree, middle), kvs_get_key(btree, key)))      \
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
    void prefix##_deinit(struct prefix* btree)                                 \
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
        kvs_deinit(btree);                                                     \
    }                                                                          \
                                                                               \
    V* prefix##_emplace_new(struct prefix** btree, K key)                      \
    {                                                                          \
        int##bits##_t idx = prefix##_lower_bound(*btree, key);                 \
        if (idx < btree_count(*btree) &&                                       \
            kvs_equal(key, kvs_get_key(*btree, idx)))                          \
            return NULL;                                                       \
                                                                               \
        return kvs_emplace(btree, idx, key);                                   \
    }                                                                          \
                                                                               \
    enum btree_status prefix##_emplace_or_get(                                 \
        struct prefix** btree, K key, V** value)                               \
    {                                                                          \
        V*            emplaced;                                                \
        int##bits##_t idx = prefix##_lower_bound(*btree, key);                 \
        if (idx < btree_count(*btree) &&                                       \
            kvs_equal(key, kvs_get_key(*btree, idx)))                          \
        {                                                                      \
            *value = kvs_get_value(*btree, idx);                               \
            return BTREE_EXISTS;                                               \
        }                                                                      \
                                                                               \
        emplaced = kvs_emplace(btree, idx, key);                               \
        if (emplaced == NULL)                                                  \
            return BTREE_OOM;                                                  \
        *value = emplaced;                                                     \
        return BTREE_NEW;                                                      \
    }                                                                          \
                                                                               \
    void prefix##_erase(struct prefix* btree, K key)                           \
    {                                                                          \
        int##bits##_t idx = prefix##_lower_bound(btree, key);                  \
        if (idx < btree_count(btree) &&                                        \
            kvs_equal(key, kvs_get_key(btree, idx)))                           \
        {                                                                      \
            kvs_erase(btree, idx);                                             \
        }                                                                      \
    }                                                                          \
                                                                               \
    V* prefix##_find(struct prefix* btree, K key)                              \
    {                                                                          \
        int##bits##_t idx = prefix##_lower_bound(btree, key);                  \
        if (idx == btree_count(btree) ||                                       \
            !kvs_equal(key, kvs_get_key(btree, idx)))                          \
            return NULL;                                                       \
        return kvs_get_value(btree, idx);                                      \
    }

#define btree_count(btree)    ((btree) ? (btree)->count : 0)
#define btree_capacity(btree) ((btree) ? (btree)->capacity : 0)

#define btree_for_each(btree, idx, key, value)                                 \
    for (idx = 0;                                                              \
         idx != btree_count(btree) && (key = (btree)->keys[idx], 1) &&         \
         (value = &(btree)->values[idx], 1);                                   \
         ++idx)

#define btree_for_each_full(                                                   \
    btree, idx, key, value, kvs_get_key, kvs_get_value)                        \
    for (idx = 0;                                                              \
         idx != btree_count(btree) && (btree = kvs_get_key(btree, idx), 1) &&  \
         (value = kvs_get_value(btree, idx), 1);                               \
         ++idx)
