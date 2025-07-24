#pragma once

#include "clither/util/hmap.h"
#include "clither/util/str.h"

#define HMAP_DECLARE_STR(API, prefix, V, bits)                                 \
    struct prefix##_kvs                                                        \
    {                                                                          \
        struct str** keys;                                                     \
        V*           values;                                                   \
    };                                                                         \
    HMAP_DECLARE_FULL(                                                         \
        API, prefix, hash32, const char*, V, bits, struct prefix##_kvs)

#define HMAP_DEFINE_STR(API, prefix, V, bits)                                  \
    static hash32 prefix##_hash(const char* key)                               \
    {                                                                          \
        return hash32_jenkins_oaat(key, strlen(key));                          \
    }                                                                          \
    static int prefix##_kvs_alloc(                                             \
        struct prefix##_kvs* kvs,                                              \
        struct prefix##_kvs* old_kvs,                                          \
        int##bits##_t        capacity)                                         \
    {                                                                          \
        int##bits##_t i;                                                       \
        (void)old_kvs;                                                         \
        if ((kvs->keys = (struct str**)mem_alloc(                              \
                 sizeof(*kvs->keys) * capacity)) == NULL)                      \
            return -1;                                                         \
        if ((kvs->values = (V*)mem_alloc(sizeof(V) * capacity)) == NULL)       \
        {                                                                      \
            mem_free(kvs->keys);                                               \
            return -1;                                                         \
        }                                                                      \
        for (i = 0; i != capacity; ++i)                                        \
            str_init(&kvs->keys[i]);                                           \
        return 0;                                                              \
    }                                                                          \
    static void prefix##_kvs_free(                                             \
        struct prefix##_kvs* kvs, int##bits##_t capacity)                      \
    {                                                                          \
        int##bits##_t i;                                                       \
        mem_free(kvs->values);                                                 \
                                                                               \
        for (i = 0; i != capacity; ++i)                                        \
            str_deinit(kvs->keys[i]);                                          \
        mem_free(kvs->keys);                                                   \
    }                                                                          \
    static void prefix##_kvs_free_old(                                         \
        struct prefix##_kvs* kvs, int##bits##_t capacity)                      \
    {                                                                          \
        prefix##_kvs_free(kvs, capacity);                                      \
    }                                                                          \
    static const char* prefix##_kvs_get_key(                                   \
        const struct prefix##_kvs* kvs, int##bits##_t slot)                    \
    {                                                                          \
        return str_cstr(kvs->keys[slot]);                                      \
    }                                                                          \
    static void prefix##_kvs_set_key(                                          \
        struct prefix##_kvs* kvs, int##bits##_t slot, const char* key)         \
    {                                                                          \
        str_set_cstr(&kvs->keys[slot], key);                                   \
    }                                                                          \
    static int prefix##_kvs_keys_equal(const char* k1, const char* k2)         \
    {                                                                          \
        return strcmp(k1, k2) == 0;                                            \
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
        hash32,                                                                \
        const char*,                                                           \
        V,                                                                     \
        bits,                                                                  \
        prefix##_hash,                                                         \
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
