#include "clither/server/food_in_proximity_hset.h"
#include "clither/util/morton.h"

static hash32 food_in_proximity_hset_hash(morton key)
{
    return hash32_combine(key & 0xFFFFFFFF, key >> 32);
}

HSET_DEFINE_HASH(
    food_in_proximity_hset, hash32, morton, 32, food_in_proximity_hset_hash)
