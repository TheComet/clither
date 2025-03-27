#include "clither/food_acks_hmap.h"
#include "clither/morton.h"

static hash32 food_acks_hmap_hash(struct qwpos key)
{
    return (hash32)morton_encode_qwpos(key);
}

static int food_acks_hmap_kvs_alloc(
    struct food_acks_hmap_kvs* kvs,
    struct food_acks_hmap_kvs* old_kvs,
    int32_t                    capacity)
{
    (void)old_kvs;
    if ((kvs->keys = mem_alloc(sizeof(struct qwpos) * capacity)) == NULL)
        return -1;
    return 0;
}

static void food_acks_hmap_kvs_free(struct food_acks_hmap_kvs* kvs)
{
    mem_free(kvs->keys);
}

static void food_acks_hmap_kvs_free_old(struct food_acks_hmap_kvs* kvs)
{
    food_acks_hmap_kvs_free(kvs);
}

static struct qwpos
food_acks_hmap_kvs_get_key(const struct food_acks_hmap_kvs* kvs, int32_t slot)
{
    return kvs->keys[slot];
}

static void food_acks_hmap_kvs_set_key(
    struct food_acks_hmap_kvs* kvs, int32_t slot, struct qwpos key)
{
    kvs->keys[slot] = key;
}

static int food_acks_hmap_kvs_keys_equal(struct qwpos k1, struct qwpos k2)
{
    return k1.x == k2.x && k1.y == k2.y;
}

HSET_DEFINE_FULL(
    food_acks_hset,
    hash32,
    struct qwpos,
    32,
    food_acks_hmap_hash,
    food_acks_hmap_kvs_alloc,
    food_acks_hmap_kvs_free_old,
    food_acks_hmap_kvs_free,
    food_acks_hmap_kvs_get_key,
    food_acks_hmap_kvs_set_key,
    food_acks_hmap_kvs_keys_equal,
    128,
    70)
