#pragma once

#include "clither/bmap.h"

struct bezier_knot_rb;

BMAP_DECLARE(bezier_knot_acks_bmap, int16_t, char, 16)

void bezier_knot_acks_bmap_remove_stale_knots(
    struct bezier_knot_acks_bmap* bmap, const struct bezier_knot_rb* knots_rb);
