#pragma once

#include "clither/hash.h"
#include "clither/log.h" /* log_oom */
#include "clither/mem.h" /* mem_alloc, mem_free */
#include <stddef.h>      /* offsetof */

#define HM_SLOT_UNUSED 0 /* SLOT_UNUSED must be 0 for memset() to work */
#define HM_SLOT_RIP    1

enum hm_status
{
    HM_OOM = -1,
    HM_EXISTS = 0,
    HM_NEW = 1
};

#define HM_DECLARE(prefix, K, V, bits)                                         \
    HM_DECLARE_HASH(prefix, hash32, K, V, bits)

#define HM_DECLARE_HASH(prefix, H, K, V, bits)                                 \
    /* Default key-value storage malloc()'s two arrays */                      \
    struct prefix##_kvs                                                        \
    {                                                                          \
        K* keys;                                                               \
        V* values;                                                             \
    };                                                                         \
    HM_DECLARE_FULL(prefix, H, K, V, bits, struct prefix##_kvs)

#define HM_DECLARE_FULL(prefix, H, K, V, bits, KVS)                            \
    struct prefix                                                              \
    {                                                                          \
        KVS           kvs;                                                     \
        int##bits##_t count, capacity;                                         \
        H             hashes[1];                                               \
    };                                                                         \
                                                                               \
    /*! @brief Call this before using any other functions. */                  \
    static void prefix##_init(struct prefix** hm)                              \
    {                                                                          \
        *hm = NULL;                                                            \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Destroys an existing hashmap and frees all memory allocated by   \
     * inserted elements.                                                      \
     */                                                                        \
    void prefix##_deinit(struct prefix* hm);                                   \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and allocates space   \
     * for a new value.                                                        \
     * @param[in] hm Pointer to an initialized hashmap.                        \
     * @param[in] key The key to insert.                                       \
     * @return A pointer to the allocated value. If the key already exists, or \
     * if allocation fails, NULL is returned.                                  \
     */                                                                        \
    V* prefix##_emplace_new(struct prefix** hm, K key);                        \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and allocates space   \
     * for a new value. If the key exists, returns the existing value.         \
     * @param[in] hm Pointer to an initialized hashmap.                        \
     * @param[in] key The key to insert or find.                               \
     * @param[out] value Address of the existing value, or address of the      \
     * newly allocated value is written to this parameter.                     \
     * @return Returns HM_OOM if allocation fails. Returns HM_EXISTS if the    \
     * key exists and value will point to the existing value. Returns HM_NEW   \
     * if the key did not exist and was inserted and value will point to a     \
     * newly allocated value.                                                  \
     */                                                                        \
    enum hm_status prefix##_emplace_or_get(                                    \
        struct prefix** hm, K key, V** value);                                 \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and value.            \
     * @param[in] hm Pointer to an initialized hashmap.                        \
     * @param[in] key The key to insert.                                       \
     * @param[in] value The value to insert, if the key does not exist.        \
     * @return Returns 0 if the key+value was inserted. Returns -1 if the key  \
     * existed, or if allocation failed.                                       \
     */                                                                        \
    static int prefix##_insert_new(struct prefix** hm, K key, V value)         \
    {                                                                          \
        V* emplaced = prefix##_emplace_new(hm, key);                           \
        if (emplaced == NULL)                                                  \
            return -1;                                                         \
        *emplaced = value;                                                     \
        return 0;                                                              \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief If the key does not exist, inserts the key and value. If the key \
     * exists, then the existing value is overwritten with the new value.      \
     * @param[in] hm Pointer to an initialized hashmap.                        \
     * @param[in] key The key to insert or find.                               \
     * @param[in] value The value to insert or update.                         \
     * @return Returns -1 if allocation failed. Returns 0 otherwise.           \
     */                                                                        \
    static int prefix##_insert_update(struct prefix** hm, K key, V value)      \
    {                                                                          \
        V*             ins_value;                                              \
        enum hm_status status = prefix##_emplace_or_get(hm, key, &ins_value);  \
        if (status != HM_OOM)                                                  \
            *ins_value = value;                                                \
        return 0;                                                              \
    }                                                                          \
                                                                               \
    /*!                                                                        \
     * @brief Removes the key and its associated value from the hashmap.       \
     * @param[in] hm Pointer to an initialized hashmap.                        \
     * @param[in] key The key to erase.                                        \
     * @return If the key exists, returns a pointer to the value that was      \
     * removed. It is valid to read/write to this pointer until the next       \
     * hashmap operation. If the key does not exist, returns NULL.             \
     */                                                                        \
    V* prefix##_erase(struct prefix* hm, K key);                               \
                                                                               \
    /*!                                                                        \
     * @brief Finds the value associated with the specified key.               \
     * @param[in] hm Pointer to an initialized hashmap.                        \
     * @param[in] key The key to find.                                         \
     * @return If the key exists, a pointer to the value is returned. It is    \
     * valid to read/write to the value until the next hashmap operation. If   \
     * the key does not exist, NULL is returned.                               \
     */                                                                        \
    V* prefix##_find(const struct prefix* hm, K key);

#define HM_DEFINE(prefix, K, V, bits)                                          \
    static hash32 prefix##_hash(K key)                                         \
    {                                                                          \
        return hash32_jenkins_oaat(&key, sizeof(K));                           \
    }                                                                          \
    HM_DEFINE_HASH(prefix, hash32, K, V, bits, prefix##_hash)

#define HM_DEFINE_HASH(prefix, H, K, V, bits, hash_func)                       \
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
    HM_DEFINE_FULL(                                                            \
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

#define HM_DEFINE_FULL(                                                        \
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
    void prefix##_deinit(struct prefix* hm)                                    \
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
        if (hm != NULL)                                                        \
        {                                                                      \
            kvs_free(&hm->kvs);                                                \
            mem_free(hm);                                                      \
        }                                                                      \
    }                                                                          \
                                                                               \
    static int##bits##_t prefix##_find_slot(                                   \
        const struct prefix* hm, K key, H h);                                  \
    static int prefix##_grow(struct prefix** hm)                               \
    {                                                                          \
        struct prefix* new_hm;                                                 \
        int##bits##_t  i, header, data;                                        \
        int##bits##_t  old_cap = *hm ? (*hm)->capacity : 0;                    \
        int##bits##_t  new_cap = old_cap ? old_cap * 2 : MIN_CAPACITY;         \
        /* Must be power of 2 */                                               \
        CLITHER_DEBUG_ASSERT((new_cap & (new_cap - 1)) == 0);                  \
                                                                               \
        header = offsetof(struct prefix, hashes);                              \
        data = sizeof((*hm)->hashes[0]) * new_cap;                             \
        new_hm = (struct prefix*)mem_alloc(header + data);                     \
        if (new_hm == NULL)                                                    \
            goto alloc_hm_failed;                                              \
        if (kvs_alloc(&new_hm->kvs, &(*hm)->kvs, new_cap) != 0)                \
            goto alloc_storage_failed;                                         \
                                                                               \
        /* NOTE: Relies on HM_SLOT_UNUSED being 0 */                           \
        memset(new_hm->hashes, 0, sizeof(H) * new_cap);                        \
        new_hm->count = 0;                                                     \
        new_hm->capacity = new_cap;                                            \
                                                                               \
        /* This should never fail so we don't error check */                   \
        for (i = 0; i != old_cap; ++i)                                         \
        {                                                                      \
            int##bits##_t slot;                                                \
            H             h;                                                   \
            if ((*hm)->hashes[i] == HM_SLOT_UNUSED ||                          \
                (*hm)->hashes[i] == HM_SLOT_RIP)                               \
                continue;                                                      \
                                                                               \
            /* We use two reserved values for hashes. The hash function could  \
             * produce them, which would mess up collision resolution */       \
            h = hash_func(kvs_get_key(&(*hm)->kvs, i));                        \
            if (h == HM_SLOT_UNUSED || h == HM_SLOT_RIP)                       \
                h = 2;                                                         \
            slot = prefix##_find_slot(new_hm, kvs_get_key(&(*hm)->kvs, i), h); \
            CLITHER_DEBUG_ASSERT(slot >= 0);                                   \
            new_hm->hashes[slot] = h;                                          \
            kvs_set_key(&new_hm->kvs, slot, kvs_get_key(&(*hm)->kvs, i));      \
            kvs_set_value(&new_hm->kvs, slot, kvs_get_value(&(*hm)->kvs, i));  \
            new_hm->count++;                                                   \
        }                                                                      \
                                                                               \
        /* Free old hashmap */                                                 \
        if (*hm != NULL)                                                       \
        {                                                                      \
            kvs_free_old(&(*hm)->kvs);                                         \
            mem_free(*hm);                                                     \
        }                                                                      \
        *hm = new_hm;                                                          \
                                                                               \
        return 0;                                                              \
                                                                               \
    alloc_storage_failed:                                                      \
        mem_free(new_hm);                                                      \
    alloc_hm_failed:                                                           \
        return log_oom(header + data, "hm_grow()");                            \
    }                                                                          \
    /*!                                                                        \
     * @return If key exists: -(1 + slot)                                      \
     *         If key does not exist: slot                                     \
     */                                                                        \
    static int##bits##_t prefix##_find_slot(                                   \
        const struct prefix* hm, K key, H h)                                   \
    {                                                                          \
        int##bits##_t slot, i, last_rip;                                       \
        CLITHER_DEBUG_ASSERT(hm && hm->capacity > 0);                          \
        CLITHER_DEBUG_ASSERT(h > 1);                                           \
                                                                               \
        i = 0;                                                                 \
        last_rip = -1;                                                         \
        slot = (int##bits##_t)(h & (H)(hm->capacity - 1));                     \
                                                                               \
        while (hm->hashes[slot] != HM_SLOT_UNUSED)                             \
        {                                                                      \
            /* If the same hash already exists in this slot, and this isn't    \
             * the result of a hash collision (which we can verify by          \
             * comparing the original keys), then we can conclude this key was \
             * already inserted */                                             \
            if (hm->hashes[slot] == h)                                         \
                if (kvs_keys_equal(kvs_get_key(&hm->kvs, slot), key))          \
                    return -(1 + slot);                                        \
            /* Keep track of visited tombstones, as it's possible to insert    \
             * into them */                                                    \
            if (hm->hashes[slot] == HM_SLOT_RIP)                               \
                last_rip = slot;                                               \
            /* Quadratic probing following p(K,i)=(i^2+i)/2. If the hash table \
             * size is a power of two, this will visit every slot. */          \
            i++;                                                               \
            slot = (int##bits##_t)((slot + i) & (hm->capacity - 1));           \
        }                                                                      \
                                                                               \
        /* Prefer inserting into a tombstone. Note that there is no way to     \
         * exit early when probing for insert positions, because it's not      \
         * possible to know if the key exists or not without completing the    \
         * entire probing sequence. */                                         \
        if (last_rip != -1)                                                    \
            slot = last_rip;                                                   \
                                                                               \
        return slot;                                                           \
    }                                                                          \
    V* prefix##_emplace_new(struct prefix** hm, K key)                         \
    {                                                                          \
        H             h;                                                       \
        int##bits##_t slot;                                                    \
                                                                               \
        /* NOTE: Rehashing may change table count, make sure to calculate hash \
         * after this */                                                       \
        if (!*hm || (*hm)->count * 100 >= REHASH_AT_PERCENT * (*hm)->capacity) \
            if (prefix##_grow(hm) != 0)                                        \
                return NULL;                                                   \
                                                                               \
        /* We use two reserved values for hashes. The hash function could      \
         * produce them, which would mess up collision resolution */           \
        h = hash_func(key);                                                    \
        if (h == HM_SLOT_UNUSED || h == HM_SLOT_RIP)                           \
            h = 2;                                                             \
                                                                               \
        slot = prefix##_find_slot(*hm, key, h);                                \
        if (slot < 0)                                                          \
            return NULL;                                                       \
                                                                               \
        (*hm)->count++;                                                        \
        (*hm)->hashes[slot] = h;                                               \
        kvs_set_key(&(*hm)->kvs, slot, key);                                   \
        return kvs_get_value(&(*hm)->kvs, slot);                               \
    }                                                                          \
    enum hm_status prefix##_emplace_or_get(                                    \
        struct prefix** hm, K key, V** value)                                  \
    {                                                                          \
        hash32        h;                                                       \
        int##bits##_t slot;                                                    \
                                                                               \
        /* NOTE: Rehashing may change table count, make sure to calculate hash \
         * after this */                                                       \
        if (!*hm || (*hm)->count * 100 >= REHASH_AT_PERCENT * (*hm)->capacity) \
            if (prefix##_grow(hm) != 0)                                        \
                return HM_OOM;                                                 \
                                                                               \
        /* We use two reserved values for hashes. The hash function could      \
         * produce them, which would mess up collision resolution */           \
        h = hash_func(key);                                                    \
        if (h == HM_SLOT_UNUSED || h == HM_SLOT_RIP)                           \
            h = 2;                                                             \
                                                                               \
        slot = prefix##_find_slot(*hm, key, h);                                \
        if (slot < 0)                                                          \
        {                                                                      \
            *value = kvs_get_value(&(*hm)->kvs, -1 - slot);                    \
            return HM_EXISTS;                                                  \
        }                                                                      \
                                                                               \
        (*hm)->count++;                                                        \
        (*hm)->hashes[slot] = h;                                               \
        kvs_set_key(&(*hm)->kvs, slot, key);                                   \
        *value = kvs_get_value(&(*hm)->kvs, slot);                             \
        return HM_NEW;                                                         \
    }                                                                          \
    V* prefix##_find(const struct prefix* hm, K key)                           \
    {                                                                          \
        H             h;                                                       \
        int##bits##_t slot;                                                    \
                                                                               \
        if (hm == NULL)                                                        \
            return NULL;                                                       \
                                                                               \
        /* We use two reserved values for hashes. The hash function could      \
         * produce them, which would mess up collision resolution */           \
        h = hash_func(key);                                                    \
        if (h == HM_SLOT_UNUSED || h == HM_SLOT_RIP)                           \
            h = 2;                                                             \
                                                                               \
        slot = prefix##_find_slot(hm, key, h);                                 \
        if (slot >= 0)                                                         \
            return NULL;                                                       \
                                                                               \
        return kvs_get_value(&hm->kvs, -1 - slot);                             \
    }                                                                          \
    V* prefix##_erase(struct prefix* hm, K key)                                \
    {                                                                          \
        H             h;                                                       \
        int##bits##_t slot;                                                    \
        /* We use two reserved values for hashes. The hash function could      \
         * produce them, which would mess up collision resolution */           \
        h = hash_func(key);                                                    \
        if (h == HM_SLOT_UNUSED || h == HM_SLOT_RIP)                           \
            h = 2;                                                             \
                                                                               \
        slot = prefix##_find_slot(hm, key, h);                                 \
        if (slot >= 0)                                                         \
            return NULL;                                                       \
                                                                               \
        hm->count--;                                                           \
        hm->hashes[-1 - slot] = HM_SLOT_RIP;                                   \
        return kvs_get_value(&hm->kvs, -1 - slot);                             \
    }

#define hm_count(hm)    ((hm) ? (hm)->count : 0)
#define hm_capacity(hm) ((hm) ? (hm)->capacity : 0)

static int hm_next_valid_slot(const hash32* hashes, int slot, intptr_t capacity)
{
    do
    {
        slot++;
    } while (slot < capacity &&
             (hashes[slot] == HM_SLOT_UNUSED || hashes[slot] == HM_SLOT_RIP));
    return slot;
}

#define hm_for_each(hm, slot_idx, key, value)                                  \
    for (slot_idx =                                                            \
             hm_next_valid_slot((hm)->hashes, -1, (hm) ? (hm)->capacity : 0);  \
         (hm) && slot_idx != (hm)->capacity &&                                 \
         ((key = (hm)->kvs.keys[slot_idx]) || 1) &&                            \
         ((value = &(hm)->kvs.values[slot_idx]) || 1);                         \
         slot_idx =                                                            \
             hm_next_valid_slot((hm)->hashes, slot_idx, (hm)->capacity))

#define hm_for_each_full(hm, slot_idx, key, value, kvs_get_key, kvs_get_value) \
    for (slot_idx =                                                            \
             hm_next_valid_slot((hm)->hashes, -1, (hm) ? (hm)->capacity : 0);  \
         (hm) && slot_idx != (hm)->capacity &&                                 \
         ((key = kvs_get_key(&(hm)->kvs, slot_idx)), 1) &&                     \
         ((value = kvs_get_value(&(hm)->kvs, slot_idx)), 1);                   \
         slot_idx =                                                            \
             hm_next_valid_slot((hm)->hashes, slot_idx, (hm)->capacity))
