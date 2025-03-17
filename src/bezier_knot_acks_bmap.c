#include "clither/bezier_knot_acks_bmap.h"
#include "clither/bezier_knot_rb.h"
#include "clither/rb.h"

BMAP_DEFINE(bezier_knot_acks_bmap, int16_t, char, 16)

static int retain_active_knots(int16_t knot_idx, char* ackd, void* user)
{
    const struct bezier_knot_rb* knots_rb = (const struct bezier_knot_rb*)user;
    (void)ackd;
    if (!rb_is_idx_valid_data(knots_rb, knot_idx))
        return BMAP_ERASE;
    return BMAP_RETAIN;
}

void bezier_knot_acks_bmap_remove_stale_knots(
    struct bezier_knot_acks_bmap* bmap, const struct bezier_knot_rb* knots_rb)
{
    bezier_knot_acks_bmap_retain(bmap, retain_active_knots, (void*)knots_rb);
}
