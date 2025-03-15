#include "clither/bezier_knot_acks_bmap.h"
#include "clither/proximity_state.h"

void proximity_state_init(struct proximity_state* ps)
{
    bezier_knot_acks_bmap_init(&ps->bezier_knot_acks);
}

void proximity_state_deinit(struct proximity_state* ps)
{
    bezier_knot_acks_bmap_deinit(ps->bezier_knot_acks);
}
