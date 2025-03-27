#pragma once

#include "clither/hset.h"
#include "clither/q.h"

struct food_acks_hset_kvs
{
    struct qwpos* keys;
};

HSET_DECLARE_FULL(
    food_acks_hset, hash32, struct qwpos, 32, struct food_acks_hset_kvs)
