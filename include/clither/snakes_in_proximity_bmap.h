#pragma once

#include "clither/bmap.h"

struct bezier_knot_acks_bmap;

BMAP_DECLARE(
    snakes_in_proximity_bmap, uint16_t, struct bezier_knot_acks_bmap*, 16)
