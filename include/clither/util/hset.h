#pragma once

#include "clither/util/hash.h"
#include "clither/util/log.h" /* log_oom */
#include "clither/util/mem.h" /* mem_alloc, mem_free */
#include <stddef.h>           /* offsetof */
#include <string.h>           /* memset */

#define HSET_SLOT_UNUSED 0 /* SLOT_UNUSED must be 0 for memset() to work */
#define HSET_SLOT_RIP    1

enum hset_status
{
    HSET_OOM = -1,
    HSET_EXISTS = 0,
    HSET_NEW = 1
};

#define HSET_RETAIN 0
#define HSET_ERASE  1

#if defined(CLITHER_CAPACITY_WARNING)
#    define HSET_CAPACITY_WARNING()                                            \
        do                                                                     \
        {                                                                      \
            log_warn("hset_grow(): Close to maximum capacity!\n");             \
            log_backtrace();                                                   \
        } while (0)
#else
/* clang-format off */
#    define HSET_CAPACITY_WARNING() do {} while (0)
/* clang-format on */
#endif

#define HSET_IS_POWER_OF_2(x) (((x) & ((x) - 1)) == 0)

#define HSET_DECLARE(prefix, K, bits) HSET_DECLARE_HASH(prefix, hash32, K, bits)

#define HSET_DECLARE_HASH(prefix, H, K, bits)                                  \
    /* Default key-storage implementation */                                   \
    struct prefix##_kvs                                                        \
    {                                                                          \
        K* keys;                                                               \
    };                                                                         \
    HSET_DECLARE_FULL(prefix, H, K, bits, struct prefix##_kvs)

#define HSET_DECLARE_FULL(prefix, H, K, bits, KVS)                             \
    struct prefix                                                              \
    {                                                                          \
        KVS           kvs;                                                     \
        int##bits##_t count, capacity;                                         \
        H             hashes[1];                                               \
    };                                                                         \
                                                                               \
    /*! @brief Call this before using any other functions. */                  \
    static void prefix##_init(struct prefix** hset)                            \
    {                                                                          \
        *hset = NULL;                                                          \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Destroys an existing hashset and frees all memory allocated by   \
     * inserted elements.                                                      \
     */                                                                        \
    void prefix##_deinit(struct prefix* hset);                                 \
                                                                               \
    void prefix##_clear(struct prefix* hset);                                  \
    void prefix##_compact(struct prefix** hset);                               \
                                                                               \
    /*!                                                                        \
     * @brief Inserts a key.                                                   \
     * @param[in] hset Pointer to an initialized hashset.                      \
     * @param[in] key The key to insert.                                       \
     * @return Returns HSET_OOM if allocation fails. Returns HSET_NEW if the   \
     * key did not exist. Returns HSET_EXISTS if the key existed.              \
     */                                                                        \
    enum hset_status prefix##_insert(struct prefix** hset, K key);             \
                                                                               \
    /*!                                                                        \
     * @brief Removes a key from the hashset.                                  \
     * @param[in] hset Pointer to an initialized hashset.                      \
     * @param[in] key The key to erase.                                        \
     * @return If the key exists, returns 1. If the key does not exist,        \
     * returns 0.                                                              \
     */                                                                        \
    int prefix##_erase(struct prefix* bset, K key);                            \
                                                                               \
    /*!                                                                        \
     * @brief Removes all keys for which the callback function returns         \
     * HSET_ERASE (1). Keeps all keys for which the callback function          \
     * returns HSET_RETAIN (0).                                                \
     * @param[in] hset Pointer to an initialized hset.                         \
     * @param[in] on_element Callback function. Gets called for each element   \
     * once. Return:                                                           \
     *   - HSET_RETAIN (0) to keep the element in the hset.                    \
     *   - HSET_ERASE (1) to remove the element from the hset.                 \
     *   - Any negative value to abort iterating and return an error.          \
     * @param[in] user A user defined pointer that gets passed in to the       \
     * callback function. Can be whatever you want.                            \
     * @return Returns 0 if iteration was successful. If the callback function \
     * returns a negative value, then this function will return the same       \
     * negative value. This allows propagating errors from within the callback \
     * function.                                                               \
     */                                                                        \
    int prefix##_retain(                                                       \
        struct prefix* hset,                                                   \
        int (*on_element)(K key, void* user),                                  \
        void* user);                                                           \
                                                                               \
    /*!                                                                        \
     * @brief Finds the specified key.                                         \
     * @param[in] hset Pointer to an initialized hashset.                      \
     * @param[in] key The key to find.                                         \
     * @return Returns 1 if the key exists, or 0 if the key doesn't exist.     \
     */                                                                        \
    int prefix##_find(const struct prefix* hset, K key);

#define HSET_DEFINE(prefix, K, bits)                                           \
    static hash32 prefix##_hash(K key)                                         \
    {                                                                          \
        return hash32_jenkins_oaat(&key, sizeof(K));                           \
    }                                                                          \
    HSET_DEFINE_HASH(prefix, hash32, K, bits, prefix##_hash)

#define HSET_DEFINE_HASH(prefix, H, K, bits, hash_func)                        \
    /* Default key storage implementation */                                   \
    static int prefix##_kvs_alloc(                                             \
        struct prefix##_kvs* kvs,                                              \
        struct prefix##_kvs* old_kvs,                                          \
        int##bits##_t        capacity)                                         \
    {                                                                          \
        (void)old_kvs;                                                         \
        if ((kvs->keys = (K*)mem_alloc(sizeof(K) * capacity)) == NULL)         \
            return -1;                                                         \
        return 0;                                                              \
    }                                                                          \
    static void prefix##_kvs_free(struct prefix##_kvs* kvs)                    \
    {                                                                          \
        mem_free(kvs->keys);                                                   \
    }                                                                          \
    static void prefix##_kvs_free_old(struct prefix##_kvs* kvs)                \
    {                                                                          \
        prefix##_kvs_free(kvs);                                                \
    }                                                                          \
    static K prefix##_kvs_get_key(                                             \
        const struct prefix##_kvs* kvs, int##bits##_t slot)                    \
    {                                                                          \
        return kvs->keys[slot];                                                \
    }                                                                          \
    static void prefix##_kvs_set_key(                                          \
        struct prefix##_kvs* kvs, int##bits##_t slot, K key)                   \
    {                                                                          \
        kvs->keys[slot] = key;                                                 \
    }                                                                          \
    static int prefix##_kvs_keys_equal(K k1, K k2)                             \
    {                                                                          \
        return k1 == k2;                                                       \
    }                                                                          \
    HSET_DEFINE_FULL(                                                          \
        prefix,                                                                \
        H,                                                                     \
        K,                                                                     \
        bits,                                                                  \
        hash_func,                                                             \
        prefix##_kvs_alloc,                                                    \
        prefix##_kvs_free_old,                                                 \
        prefix##_kvs_free,                                                     \
        prefix##_kvs_get_key,                                                  \
        prefix##_kvs_set_key,                                                  \
        prefix##_kvs_keys_equal,                                               \
        128,                                                                   \
        70)

#define HSET_DEFINE_FULL(                                                      \
    prefix,                                                                    \
    H,                                                                         \
    K,                                                                         \
    bits,                                                                      \
    hash_func,                                                                 \
    kvs_alloc,                                                                 \
    kvs_free_old,                                                              \
    kvs_free,                                                                  \
    kvs_get_key,                                                               \
    kvs_set_key,                                                               \
    kvs_keys_equal,                                                            \
    MIN_CAPACITY,                                                              \
    REHASH_AT_PERCENT)                                                         \
                                                                               \
    void prefix##_deinit(struct prefix* hset)                                  \
    {                                                                          \
        /* These don't do anything, except act as a poor-man's type-check for  \
         * the various key-value storage functions. */                         \
        hash32 (*hash)(K) = hash_func;                                         \
        int (*alloc)(                                                          \
            struct prefix##_kvs*, struct prefix##_kvs*, int##bits##_t) =       \
            kvs_alloc;                                                         \
        void (*free_old)(struct prefix##_kvs*) = kvs_free_old;                 \
        void (*free_)(struct prefix##_kvs*) = kvs_free;                        \
        K(*get_key)                                                            \
        (const struct prefix##_kvs*, int##bits##_t) = kvs_get_key;             \
        void (*set_key)(struct prefix##_kvs*, int##bits##_t, K) = kvs_set_key; \
        int (*keys_equal)(K, K) = kvs_keys_equal;                              \
        (void)hash;                                                            \
        (void)alloc;                                                           \
        (void)free_old;                                                        \
        (void)free_;                                                           \
        (void)get_key;                                                         \
        (void)set_key;                                                         \
        (void)keys_equal;                                                      \
                                                                               \
        if (hset != NULL)                                                      \
        {                                                                      \
            kvs_free(&hset->kvs);                                              \
            mem_free(hset);                                                    \
        }                                                                      \
    }                                                                          \
                                                                               \
    static int##bits##_t prefix##_find_slot(                                   \
        const struct prefix* hset, K key, H h);                                \
    static int prefix##_grow(struct prefix** hset)                             \
    {                                                                          \
        struct prefix* new_hset;                                               \
        int##bits##_t  i, header, data;                                        \
        int##bits##_t  old_cap = *hset ? (*hset)->capacity : 0;                \
        int##bits##_t  new_cap = old_cap ? old_cap * 2 : MIN_CAPACITY;         \
        /* Must be power of 2 */                                               \
        CLITHER_DEBUG_ASSERT((new_cap & (new_cap - 1)) == 0);                  \
                                                                               \
        if (new_cap >= (1 << (bits - 2)))                                      \
            HSET_CAPACITY_WARNING();                                           \
                                                                               \
        header = offsetof(struct prefix, hashes);                              \
        data = sizeof((*hset)->hashes[0]) * new_cap;                           \
        new_hset = (struct prefix*)mem_alloc(header + data);                   \
        if (new_hset == NULL)                                                  \
            goto alloc_hset_failed;                                            \
        if (kvs_alloc(&new_hset->kvs, &(*hset)->kvs, new_cap) != 0)            \
            goto alloc_storage_failed;                                         \
                                                                               \
        /* NOTE: Relies on HSET_SLOT_UNUSED being 0 */                         \
        memset(new_hset->hashes, 0, sizeof(H) * new_cap);                      \
        new_hset->count = 0;                                                   \
        new_hset->capacity = new_cap;                                          \
                                                                               \
        /* This should never fail so we don't error check */                   \
        for (i = 0; i != old_cap; ++i)                                         \
        {                                                                      \
            int##bits##_t slot;                                                \
            H             h;                                                   \
            if ((*hset)->hashes[i] == HSET_SLOT_UNUSED ||                      \
                (*hset)->hashes[i] == HSET_SLOT_RIP)                           \
                continue;                                                      \
                                                                               \
            /* We use two reserved values for hashes. The hash function could  \
             * produce them, which would mess up collision resolution */       \
            h = hash_func(kvs_get_key(&(*hset)->kvs, i));                      \
            if (h == HSET_SLOT_UNUSED || h == HSET_SLOT_RIP)                   \
                h = 2;                                                         \
            slot = prefix##_find_slot(                                         \
                new_hset, kvs_get_key(&(*hset)->kvs, i), h);                   \
            CLITHER_DEBUG_ASSERT(slot >= 0);                                   \
            new_hset->hashes[slot] = h;                                        \
            kvs_set_key(&new_hset->kvs, slot, kvs_get_key(&(*hset)->kvs, i));  \
            new_hset->count++;                                                 \
        }                                                                      \
                                                                               \
        /* Free old hashset */                                                 \
        if (*hset != NULL)                                                     \
        {                                                                      \
            kvs_free_old(&(*hset)->kvs);                                       \
            mem_free(*hset);                                                   \
        }                                                                      \
        *hset = new_hset;                                                      \
                                                                               \
        return 0;                                                              \
                                                                               \
    alloc_storage_failed:                                                      \
        mem_free(new_hset);                                                    \
    alloc_hset_failed:                                                         \
        return log_oom(header + data, "hset_grow()");                          \
    }                                                                          \
    /*!                                                                        \
     * @return If key exists: -(1 + slot)                                      \
     *         If key does not exist: slot                                     \
     */                                                                        \
    static int##bits##_t prefix##_find_slot(                                   \
        const struct prefix* hset, K key, H h)                                 \
    {                                                                          \
        int##bits##_t slot, i, rip;                                            \
        CLITHER_DEBUG_ASSERT(hset && hset->capacity > 0);                      \
        CLITHER_DEBUG_ASSERT(h > 1);                                           \
        CLITHER_DEBUG_ASSERT(HSET_IS_POWER_OF_2(hset->capacity));              \
                                                                               \
        i = 0;                                                                 \
        rip = -1;                                                              \
        slot = (int##bits##_t)(h & (H)(hset->capacity - 1));                   \
                                                                               \
        while (i < hset->capacity && hset->hashes[slot] != HSET_SLOT_UNUSED)   \
        {                                                                      \
            /* If the same hash already exists in this slot, and this isn't    \
             * the result of a hash collision (which we can verify by          \
             * comparing the original keys), then we can conclude this key was \
             * already inserted */                                             \
            if (hset->hashes[slot] == h)                                       \
                if (kvs_keys_equal(kvs_get_key(&hset->kvs, slot), key))        \
                    return -(1 + slot);                                        \
            /* Keep track of first tombstone if we find one */                 \
            if (rip == -1 && hset->hashes[slot] == HSET_SLOT_RIP)              \
                rip = slot;                                                    \
            /* Quadratic probing following p(K,i)=(i^2+i)/2. If the hash table \
             * size is a power of two, this will visit every slot. */          \
            i++;                                                               \
            slot = (int##bits##_t)((slot + i) & (hset->capacity - 1));         \
        }                                                                      \
                                                                               \
        /* Prefer inserting into a tombstone. Note that there is no way to     \
         * exit early when probing for insert positions, because it's not      \
         * possible to know if the key exists or not without completing the    \
         * entire probing sequence. */                                         \
        slot = rip == -1 ? slot : rip;                                         \
        CLITHER_DEBUG_ASSERT(hset->hashes[slot] < 2);                          \
        return slot;                                                           \
    }                                                                          \
    void prefix##_clear(struct prefix* hset)                                   \
    {                                                                          \
        if (hset == NULL)                                                      \
            return;                                                            \
                                                                               \
        hset->count = 0;                                                       \
        memset(hset->hashes, 0, sizeof(H) * hset->capacity);                   \
    }                                                                          \
    void prefix##_compact(struct prefix** hset)                                \
    {                                                                          \
        CLITHER_DEBUG_ASSERT(0); /* Not yet implemented */                     \
        (void)hset;                                                            \
    }                                                                          \
    enum hset_status prefix##_insert(struct prefix** hset, K key)              \
    {                                                                          \
        H             h;                                                       \
        int##bits##_t slot;                                                    \
                                                                               \
        /* NOTE: Rehashing may change table count, make sure to calculate hash \
         * after this */                                                       \
        if (!*hset ||                                                          \
            (*hset)->count * 100 >= REHASH_AT_PERCENT * (*hset)->capacity)     \
            if (prefix##_grow(hset) != 0)                                      \
                return HSET_OOM;                                               \
                                                                               \
        /* We use two reserved values for hashes. The hash function could      \
         * produce them, which would mess up collision resolution */           \
        h = hash_func(key);                                                    \
        if (h == HSET_SLOT_UNUSED || h == HSET_SLOT_RIP)                       \
            h = 2;                                                             \
                                                                               \
        slot = prefix##_find_slot(*hset, key, h);                              \
        if (slot < 0)                                                          \
            return HSET_EXISTS;                                                \
                                                                               \
        (*hset)->count++;                                                      \
        (*hset)->hashes[slot] = h;                                             \
        kvs_set_key(&(*hset)->kvs, slot, key);                                 \
        return HSET_NEW;                                                       \
    }                                                                          \
    int prefix##_erase(struct prefix* hset, K key)                             \
    {                                                                          \
        H             h;                                                       \
        int##bits##_t slot;                                                    \
        /* We use two reserved values for hashes. The hash function could      \
         * produce them, which would mess up collision resolution */           \
        h = hash_func(key);                                                    \
        if (h == HSET_SLOT_UNUSED || h == HSET_SLOT_RIP)                       \
            h = 2;                                                             \
                                                                               \
        slot = prefix##_find_slot(hset, key, h);                               \
        if (slot >= 0)                                                         \
            return 0;                                                          \
                                                                               \
        hset->count--;                                                         \
        hset->hashes[-1 - slot] = HSET_SLOT_RIP;                               \
        return 1;                                                              \
    }                                                                          \
    int prefix##_retain(                                                       \
        struct prefix* hset, int (*on_element)(K key, void* user), void* user) \
    {                                                                          \
        int##bits##_t slot;                                                    \
        if (hset == NULL)                                                      \
            return 0;                                                          \
        for (slot = 0; slot != hset->capacity; ++slot)                         \
        {                                                                      \
            int result;                                                        \
                                                                               \
            if (hset->hashes[slot] == HSET_SLOT_UNUSED ||                      \
                hset->hashes[slot] == HSET_SLOT_RIP)                           \
                continue;                                                      \
                                                                               \
            result = on_element(kvs_get_key(&hset->kvs, slot), user);          \
            if (result < 0)                                                    \
                return result;                                                 \
            if (result != HSET_RETAIN)                                         \
            {                                                                  \
                hset->count--;                                                 \
                hset->hashes[slot] = HSET_SLOT_RIP;                            \
            }                                                                  \
        }                                                                      \
        return 0;                                                              \
    }                                                                          \
    int prefix##_find(const struct prefix* hset, K key)                        \
    {                                                                          \
        H h;                                                                   \
                                                                               \
        if (hset == NULL)                                                      \
            return 0;                                                          \
                                                                               \
        /* We use two reserved values for hashes. The hash function could      \
         * produce them, which would mess up collision resolution */           \
        h = hash_func(key);                                                    \
        if (h == HSET_SLOT_UNUSED || h == HSET_SLOT_RIP)                       \
            h = 2;                                                             \
                                                                               \
        return prefix##_find_slot(hset, key, h) < 0;                           \
    }

#define hset_count(hset)    ((hset) ? (hset)->count : 0)
#define hset_capacity(hset) ((hset) ? (hset)->capacity : 0)

static int
hset_next_valid_slot(const hash32* hashes, int slot, intptr_t capacity)
{
    do
    {
        slot++;
    } while (slot < capacity && (hashes[slot] == HSET_SLOT_UNUSED ||
                                 hashes[slot] == HSET_SLOT_RIP));
    return slot;
}

#define hset_for_each(hset, slot_idx, key)                                     \
    for (slot_idx = hset_next_valid_slot(                                      \
             (hset)->hashes, -1, (hset) ? (hset)->capacity : 0);               \
         (hset) && slot_idx != (hset)->capacity &&                             \
         ((key = (hset)->kvs.keys[slot_idx]), 1);                              \
         slot_idx =                                                            \
             hset_next_valid_slot((hset)->hashes, slot_idx, (hset)->capacity))

#define hset_for_each_full(                                                    \
    hset, slot_idx, key, value, kvs_get_key, kvs_get_value)                    \
    for (slot_idx = hset_next_valid_slot(                                      \
             (hset)->hashes, -1, (hset) ? (hset)->capacity : 0);               \
         (hset) && slot_idx != (hset)->capacity &&                             \
         ((key = kvs_get_key(&(hset)->kvs, slot_idx)), 1);                     \
         slot_idx =                                                            \
             hset_next_valid_slot((hset)->hashes, slot_idx, (hset)->capacity))
