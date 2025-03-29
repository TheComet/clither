#include "clither/server/food_in_proximity_hset.h"

static hash32 food_in_proximity_hset_hash(uint64_t key)
{
    return hash32_combine(key & 0xFFFFFFFF, key >> 32);
}

HSET_DEFINE_HASH(
    food_in_proximity_hset, hash32, uint64_t, 32, food_in_proximity_hset_hash)
