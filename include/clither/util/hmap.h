#pragma once

#include "clither/util/hash.h"
#include "clither/util/log.h" /* log_oom */
#include "clither/util/mem.h" /* mem_alloc, mem_free */
#include <stddef.h>           /* offsetof */
#include <string.h>           /* memset */

enum hmap_status
{
    HMAP_OOM = -1,
    HMAP_EXISTS = 0,
    HMAP_NEW = 1
};

#define HMAP_SLOT_UNUSED 0 /* SLOT_UNUSED must be 0 for memset() to work */
#define HMAP_SLOT_RIP    1

#if defined(CLITHER_CAPACITY_WARNING)
#    define HMAP_CAPACITY_WARNING()                                            \
        do                                                                     \
        {                                                                      \
            log_warn("hmap_grow(): Close to maximum capacity!\n");             \
            log_backtrace();                                                   \
        } while (0)
#else
/* clang-format off */
#    define HMAP_CAPACITY_WARNING() do {} while (0)
/* clang-format on */
#endif

#define HMAP_IS_POWER_OF_2(x) (((x) & ((x) - 1)) == 0)

#define HMAP_DECLARE(API, prefix, K, V, bits)                                  \
    HMAP_DECLARE_HASH(API, prefix, hash32, K, V, bits)

#define HMAP_DECLARE_HASH(API, prefix, H, K, V, bits)                          \
    /* Default key-value storage malloc()'s two arrays */                      \
    struct prefix##_kvs                                                        \
    {                                                                          \
        K* keys;                                                               \
        V* values;                                                             \
    };                                                                         \
    HMAP_DECLARE_FULL(API, prefix, H, K, V, bits, struct prefix##_kvs)

#define HMAP_DECLARE_FULL(API, prefix, H, K, V, bits, KVS)                     \
    struct prefix                                                              \
    {                                                                          \
        KVS           kvs;                                                     \
        int##bits##_t count, capacity;                                         \
        H             hashes[1];                                               \
    };                                                                         \
                                                                               \
    /*! @brief Call this before using any other functions. */                  \
    static void prefix##_init(struct prefix** hmap)                            \
    {                                                                          \
        *hmap = NULL;                                                          \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Destroys an existing hashmap and frees all memory allocated by   \
     * inserted elements.                                                      \
     */                                                                        \
    API void prefix##_deinit(struct prefix* hmap);                             \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and allocates space   \
     * for a new value.                                                        \
     * @param[in] hmap Pointer to an initialized hashmap.                      \
     * @param[in] key The key to insert.                                       \
     * @return A pointer to the allocated value. If the key already exists, or \
     * if allocation fails, NULL is returned.                                  \
     */                                                                        \
    API V* prefix##_emplace_new(struct prefix** hmap, K key);                  \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and allocates space   \
     * for a new value. If the key exists, returns the existing value.         \
     * @param[in] hmap Pointer to an initialized hashmap.                      \
     * @param[in] key The key to insert or find.                               \
     * @param[out] value Address of the existing value, or address of the      \
     * newly allocated value is written to this parameter.                     \
     * @return Returns HMAP_OOM if allocation fails. Returns HMAP_EXISTS if    \
     * the key exists and value will point to the existing value. Returns      \
     * HMAP_NEW if the key did not exist and was inserted and value will point \
     * to a newly allocated value.                                             \
     */                                                                        \
    API enum hmap_status prefix##_emplace_or_get(                              \
        struct prefix** hmap, K key, V** value);                               \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and value.            \
     * @param[in] hmap Pointer to an initialized hashmap.                      \
     * @param[in] key The key to insert.                                       \
     * @param[in] value The value to insert, if the key does not exist.        \
     * @return Returns 0 if the key+value was inserted. Returns -1 if the key  \
     * existed, or if allocation failed.                                       \
     */                                                                        \
    static int prefix##_insert_new(struct prefix** hmap, K key, V value)       \
    {                                                                          \
        V* emplaced = prefix##_emplace_new(hmap, key);                         \
        if (emplaced == NULL)                                                  \
            return -1;                                                         \
        *emplaced = value;                                                     \
        return 0;                                                              \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and value. If the key \
     * exists, then the existing value is overwritten with the new value.      \
     * @param[in] hmap Pointer to an initialized hashmap.                      \
     * @param[in] key The key to insert or find.                               \
     * @param[in] value The value to insert or update.                         \
     * @return Returns -1 if allocation failed. Returns 0 otherwise.           \
     */                                                                        \
    static int prefix##_insert_update(struct prefix** hmap, K key, V value)    \
    {                                                                          \
        V*               ins_value;                                            \
        enum hmap_status status =                                              \
            prefix##_emplace_or_get(hmap, key, &ins_value);                    \
        if (status != HMAP_OOM)                                                \
            *ins_value = value;                                                \
        return 0;                                                              \
    }                                                                          \
                                                                               \
    API V* prefix##_erase_slot(struct prefix* hmap, int##bits##_t slot);       \
                                                                               \
    /*!                                                                        \
     * @brief Removes the key and its associated value from the hashmap.       \
     * @param[in] hmap Pointer to an initialized hashmap.                      \
     * @param[in] key The key to erase.                                        \
     * @return If the key exists, returns a pointer to the value that was      \
     * removed. It is valid to read/write to this pointer until the next       \
     * hashmap operation. If the key does not exist, returns NULL.             \
     */                                                                        \
    API V* prefix##_erase(struct prefix* hmap, K key);                         \
                                                                               \
    /*!                                                                        \
     * @brief Finds the value associated with the specified key.               \
     * @param[in] hmap Pointer to an initialized hashmap.                      \
     * @param[in] key The key to find.                                         \
     * @return If the key exists, a pointer to the value is returned. It is    \
     * valid to read/write to the value until the next hashmap operation. If   \
     * the key does not exist, NULL is returned.                               \
     */                                                                        \
    API V* prefix##_find(const struct prefix* hmap, K key);

#define HMAP_DEFINE(API, prefix, K, V, bits)                                   \
    static hash32 prefix##_hash(K key)                                         \
    {                                                                          \
        return hash32_jenkins_oaat(&key, sizeof(K));                           \
    }                                                                          \
    HMAP_DEFINE_HASH(API, prefix, hash32, K, V, bits, prefix##_hash)

#define HMAP_DEFINE_HASH(API, prefix, H, K, V, bits, hash_func)                \
    /* Default key-value storage implementation */                             \
    static int prefix##_kvs_alloc(                                             \
        struct prefix##_kvs* kvs,                                              \
        struct prefix##_kvs* old_kvs,                                          \
        int##bits##_t        capacity)                                         \
    {                                                                          \
        (void)old_kvs;                                                         \
        if ((kvs->keys = (K*)mem_alloc(sizeof(K) * capacity)) == NULL)         \
            return -1;                                                         \
        if ((kvs->values = (V*)mem_alloc(sizeof(V) * capacity)) == NULL)       \
        {                                                                      \
            mem_free(kvs->keys);                                               \
            return -1;                                                         \
        }                                                                      \
                                                                               \
        return 0;                                                              \
    }                                                                          \
    static void prefix##_kvs_free(struct prefix##_kvs* kvs)                    \
    {                                                                          \
        mem_free(kvs->values);                                                 \
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
    static V* prefix##_kvs_get_value(                                          \
        const struct prefix##_kvs* kvs, int##bits##_t slot)                    \
    {                                                                          \
        return &kvs->values[slot];                                             \
    }                                                                          \
    static void prefix##_kvs_set_value(                                        \
        struct prefix##_kvs* kvs, int##bits##_t slot, const V* value)          \
    {                                                                          \
        kvs->values[slot] = *value;                                            \
    }                                                                          \
    HMAP_DEFINE_FULL(                                                          \
        API,                                                                   \
        prefix,                                                                \
        H,                                                                     \
        K,                                                                     \
        V,                                                                     \
        bits,                                                                  \
        hash_func,                                                             \
        prefix##_kvs_alloc,                                                    \
        prefix##_kvs_free_old,                                                 \
        prefix##_kvs_free,                                                     \
        prefix##_kvs_get_key,                                                  \
        prefix##_kvs_set_key,                                                  \
        prefix##_kvs_keys_equal,                                               \
        prefix##_kvs_get_value,                                                \
        prefix##_kvs_set_value,                                                \
        128,                                                                   \
        70)

#define HMAP_DEFINE_FULL(                                                      \
    API,                                                                       \
    prefix,                                                                    \
    H,                                                                         \
    K,                                                                         \
    V,                                                                         \
    bits,                                                                      \
    hash_func,                                                                 \
    kvs_alloc,                                                                 \
    kvs_free_old,                                                              \
    kvs_free,                                                                  \
    kvs_get_key,                                                               \
    kvs_set_key,                                                               \
    kvs_keys_equal,                                                            \
    kvs_get_value,                                                             \
    kvs_set_value,                                                             \
    MIN_CAPACITY,                                                              \
    REHASH_AT_PERCENT)                                                         \
                                                                               \
    API void prefix##_deinit(struct prefix* hmap)                              \
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
        V* (*get_value)(const struct prefix##_kvs*, int##bits##_t) =           \
            kvs_get_value;                                                     \
        void (*set_value)(struct prefix##_kvs*, int##bits##_t, const V*) =     \
            kvs_set_value;                                                     \
        (void)hash;                                                            \
        (void)alloc;                                                           \
        (void)free_old;                                                        \
        (void)free_;                                                           \
        (void)get_key;                                                         \
        (void)set_key;                                                         \
        (void)keys_equal;                                                      \
        (void)get_value;                                                       \
        (void)set_value;                                                       \
                                                                               \
        if (hmap != NULL)                                                      \
        {                                                                      \
            kvs_free(&hmap->kvs);                                              \
            mem_free(hmap);                                                    \
        }                                                                      \
    }                                                                          \
                                                                               \
    static int##bits##_t prefix##_find_slot(                                   \
        const struct prefix* hmap, K key, H h);                                \
    static int prefix##_grow(struct prefix** hmap)                             \
    {                                                                          \
        struct prefix* new_hmap;                                               \
        int##bits##_t  i, header, data;                                        \
        int##bits##_t  old_cap = *hmap ? (*hmap)->capacity : 0;                \
        int##bits##_t  new_cap = old_cap ? old_cap * 2 : MIN_CAPACITY;         \
        /* Must be power of 2 */                                               \
        CLITHER_DEBUG_ASSERT((new_cap & (new_cap - 1)) == 0);                  \
                                                                               \
        if (new_cap > (1 << (bits - 2)))                                       \
            HMAP_CAPACITY_WARNING();                                           \
                                                                               \
        header = offsetof(struct prefix, hashes);                              \
        data = sizeof((*hmap)->hashes[0]) * new_cap;                           \
        new_hmap = (struct prefix*)mem_alloc(header + data);                   \
        if (new_hmap == NULL)                                                  \
            goto alloc_hmap_failed;                                            \
        if (kvs_alloc(&new_hmap->kvs, &(*hmap)->kvs, new_cap) != 0)            \
            goto alloc_storage_failed;                                         \
                                                                               \
        /* NOTE: Relies on HMAP_SLOT_UNUSED being 0 */                         \
        memset(new_hmap->hashes, 0, sizeof(H) * new_cap);                      \
        new_hmap->count = 0;                                                   \
        new_hmap->capacity = new_cap;                                          \
                                                                               \
        /* This should never fail so we don't error check */                   \
        for (i = 0; i != old_cap; ++i)                                         \
        {                                                                      \
            int##bits##_t slot;                                                \
            H             h;                                                   \
            if ((*hmap)->hashes[i] == HMAP_SLOT_UNUSED ||                      \
                (*hmap)->hashes[i] == HMAP_SLOT_RIP)                           \
                continue;                                                      \
                                                                               \
            /* We use two reserved values for hashes. The hash function could  \
             * produce them, which would mess up collision resolution */       \
            h = hash_func(kvs_get_key(&(*hmap)->kvs, i));                      \
            if (h == HMAP_SLOT_UNUSED || h == HMAP_SLOT_RIP)                   \
                h = 2;                                                         \
            slot = prefix##_find_slot(                                         \
                new_hmap, kvs_get_key(&(*hmap)->kvs, i), h);                   \
            CLITHER_DEBUG_ASSERT(slot >= 0);                                   \
            new_hmap->hashes[slot] = h;                                        \
            kvs_set_key(&new_hmap->kvs, slot, kvs_get_key(&(*hmap)->kvs, i));  \
            kvs_set_value(                                                     \
                &new_hmap->kvs, slot, kvs_get_value(&(*hmap)->kvs, i));        \
            new_hmap->count++;                                                 \
        }                                                                      \
                                                                               \
        /* Free old hashmap */                                                 \
        if (*hmap != NULL)                                                     \
        {                                                                      \
            kvs_free_old(&(*hmap)->kvs);                                       \
            mem_free(*hmap);                                                   \
        }                                                                      \
        *hmap = new_hmap;                                                      \
                                                                               \
        return 0;                                                              \
                                                                               \
    alloc_storage_failed:                                                      \
        mem_free(new_hmap);                                                    \
    alloc_hmap_failed:                                                         \
        return log_oom(header + data, "hmap_grow()");                          \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @return If key exists: -(1 + slot)                                      \
     *         If key does not exist: slot                                     \
     */                                                                        \
    static int##bits##_t prefix##_find_slot(                                   \
        const struct prefix* hmap, K key, H h)                                 \
    {                                                                          \
        int##bits##_t slot, i, rip;                                            \
        CLITHER_DEBUG_ASSERT(hmap && hmap->capacity > 0);                      \
        CLITHER_DEBUG_ASSERT(h > 1);                                           \
        CLITHER_DEBUG_ASSERT(HMAP_IS_POWER_OF_2(hmap->capacity));              \
                                                                               \
        i = 0;                                                                 \
        rip = -1;                                                              \
        slot = (int##bits##_t)(h & (H)(hmap->capacity - 1));                   \
                                                                               \
        while (i < hmap->capacity && hmap->hashes[slot] != HMAP_SLOT_UNUSED)   \
        {                                                                      \
            /* If the same hash already exists in this slot, and this isn't    \
             * the result of a hash collision (which we can verify by          \
             * comparing the original keys), then we can conclude this key was \
             * already inserted */                                             \
            if (hmap->hashes[slot] == h)                                       \
                if (kvs_keys_equal(kvs_get_key(&hmap->kvs, slot), key))        \
                    return -(1 + slot);                                        \
            /* Keep track of first tombstone if we find one */                 \
            if (rip == -1 && hmap->hashes[slot] == HMAP_SLOT_RIP)              \
                rip = slot;                                                    \
            /* Quadratic probing following p(K,i)=(i^2+i)/2. If the hash table \
             * size is a power of two, this will visit every slot. */          \
            i++;                                                               \
            slot = (int##bits##_t)((slot + i) & (hmap->capacity - 1));         \
        }                                                                      \
                                                                               \
        /* Prefer inserting into a tombstone. Note that there is no way to     \
         * exit early when probing for insert positions, because it's not      \
         * possible to know if the key exists or not without completing the    \
         * entire probing sequence. */                                         \
        slot = rip == -1 ? slot : rip;                                         \
        CLITHER_DEBUG_ASSERT(hmap->hashes[slot] < 2);                          \
        return slot;                                                           \
    }                                                                          \
                                                                               \
    API V* prefix##_emplace_new(struct prefix** hmap, K key)                   \
    {                                                                          \
        H             h;                                                       \
        int##bits##_t slot;                                                    \
                                                                               \
        /* NOTE: Rehashing may change table count, make sure to calculate hash \
         * after this */                                                       \
        if (!*hmap ||                                                          \
            (*hmap)->count * 100 >= REHASH_AT_PERCENT * (*hmap)->capacity)     \
            if (prefix##_grow(hmap) != 0)                                      \
                return NULL;                                                   \
                                                                               \
        /* We use two reserved values for hashes. The hash function could      \
         * produce them, which would mess up collision resolution */           \
        h = hash_func(key);                                                    \
        if (h == HMAP_SLOT_UNUSED || h == HMAP_SLOT_RIP)                       \
            h = 2;                                                             \
                                                                               \
        slot = prefix##_find_slot(*hmap, key, h);                              \
        if (slot < 0)                                                          \
            return NULL;                                                       \
                                                                               \
        (*hmap)->count++;                                                      \
        (*hmap)->hashes[slot] = h;                                             \
        kvs_set_key(&(*hmap)->kvs, slot, key);                                 \
        return kvs_get_value(&(*hmap)->kvs, slot);                             \
    }                                                                          \
                                                                               \
    API enum hmap_status prefix##_emplace_or_get(                              \
        struct prefix** hmap, K key, V** value)                                \
    {                                                                          \
        hash32        h;                                                       \
        int##bits##_t slot;                                                    \
                                                                               \
        /* NOTE: Rehashing may change table count, make sure to calculate hash \
         * after this */                                                       \
        if (!*hmap ||                                                          \
            (*hmap)->count * 100 >= REHASH_AT_PERCENT * (*hmap)->capacity)     \
            if (prefix##_grow(hmap) != 0)                                      \
                return HMAP_OOM;                                               \
                                                                               \
        /* We use two reserved values for hashes. The hash function could      \
         * produce them, which would mess up collision resolution */           \
        h = hash_func(key);                                                    \
        if (h == HMAP_SLOT_UNUSED || h == HMAP_SLOT_RIP)                       \
            h = 2;                                                             \
                                                                               \
        slot = prefix##_find_slot(*hmap, key, h);                              \
        if (slot < 0)                                                          \
        {                                                                      \
            *value = kvs_get_value(&(*hmap)->kvs, -1 - slot);                  \
            return HMAP_EXISTS;                                                \
        }                                                                      \
                                                                               \
        (*hmap)->count++;                                                      \
        (*hmap)->hashes[slot] = h;                                             \
        kvs_set_key(&(*hmap)->kvs, slot, key);                                 \
        *value = kvs_get_value(&(*hmap)->kvs, slot);                           \
        return HMAP_NEW;                                                       \
    }                                                                          \
                                                                               \
    API V* prefix##_find(const struct prefix* hmap, K key)                     \
    {                                                                          \
        H             h;                                                       \
        int##bits##_t slot;                                                    \
                                                                               \
        if (hmap == NULL)                                                      \
            return NULL;                                                       \
                                                                               \
        /* We use two reserved values for hashes. The hash function could      \
         * produce them, which would mess up collision resolution */           \
        h = hash_func(key);                                                    \
        if (h == HMAP_SLOT_UNUSED || h == HMAP_SLOT_RIP)                       \
            h = 2;                                                             \
                                                                               \
        slot = prefix##_find_slot(hmap, key, h);                               \
        if (slot >= 0)                                                         \
            return NULL;                                                       \
                                                                               \
        return kvs_get_value(&hmap->kvs, -1 - slot);                           \
    }                                                                          \
                                                                               \
    API V* prefix##_erase_slot(struct prefix* hmap, int##bits##_t slot)        \
    {                                                                          \
        CLITHER_DEBUG_ASSERT(hmap && hmap->capacity > 0);                      \
        CLITHER_DEBUG_ASSERT(slot >= 0 && slot < hmap->capacity);              \
                                                                               \
        hmap->count--;                                                         \
        hmap->hashes[slot] = HMAP_SLOT_RIP;                                    \
                                                                               \
        return kvs_get_value(&hmap->kvs, slot);                                \
    }                                                                          \
                                                                               \
    API V* prefix##_erase(struct prefix* hmap, K key)                          \
    {                                                                          \
        H             h;                                                       \
        int##bits##_t slot;                                                    \
        /* We use two reserved values for hashes. The hash function could      \
         * produce them, which would mess up collision resolution */           \
        h = hash_func(key);                                                    \
        if (h == HMAP_SLOT_UNUSED || h == HMAP_SLOT_RIP)                       \
            h = 2;                                                             \
                                                                               \
        slot = prefix##_find_slot(hmap, key, h);                               \
        if (slot >= 0)                                                         \
            return NULL;                                                       \
                                                                               \
        return prefix##_erase_slot(hmap, -1 - slot);                           \
    }

#define hmap_count(hmap)    ((hmap) ? (hmap)->count : 0)
#define hmap_capacity(hmap) ((hmap) ? (hmap)->capacity : 0)

static int
hmap_next_valid_slot(const hash32* hashes, int slot, intptr_t capacity)
{
    do
    {
        slot++;
    } while (slot < capacity && (hashes[slot] == HMAP_SLOT_UNUSED ||
                                 hashes[slot] == HMAP_SLOT_RIP));
    return slot;
}

#define hmap_for_each(hmap, slot_idx, key, value)                              \
    for (slot_idx = hmap_next_valid_slot(                                      \
             (hmap)->hashes, -1, (hmap) ? (hmap)->capacity : 0);               \
         (hmap) && slot_idx != (hmap)->capacity &&                             \
         ((key = (hmap)->kvs.keys[slot_idx]), 1) &&                            \
         ((value = &(hmap)->kvs.values[slot_idx]), 1);                         \
         slot_idx =                                                            \
             hmap_next_valid_slot((hmap)->hashes, slot_idx, (hmap)->capacity))

#define hmap_for_each_full(                                                    \
    hmap, slot_idx, key, value, kvs_get_key, kvs_get_value)                    \
    for (slot_idx = hmap_next_valid_slot(                                      \
             (hmap)->hashes, -1, (hmap) ? (hmap)->capacity : 0);               \
         (hmap) && slot_idx != (hmap)->capacity &&                             \
         ((key = kvs_get_key(&(hmap)->kvs, slot_idx)), 1) &&                   \
         ((value = kvs_get_value(&(hmap)->kvs, slot_idx)), 1);                 \
         slot_idx =                                                            \
             hmap_next_valid_slot((hmap)->hashes, slot_idx, (hmap)->capacity))
