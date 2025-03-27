#pragma once

#include "clither/hmap.h"
#include "clither/q.h"

struct food_acks_hset_kvs
{
    struct qwpos* keys;
};

HMAP_DECLARE_FULL(
    food_acks_hmap, hash32, struct qwpos, char, 32, struct food_acks_hset_kvs)
