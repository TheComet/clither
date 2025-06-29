#pragma once

#include "clither/util/morton.h"
#include "clither/util/hset.h"

HSET_DECLARE_HASH(food_in_proximity_hset, hash32, morton, 32)
