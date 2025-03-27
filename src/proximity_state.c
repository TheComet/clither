#include "clither/bezier_knot_acks_bmap.h"
#include "clither/food_acks_hset.h"
#include "clither/proximity_state.h"

void proximity_state_init(struct proximity_state* ps)
{
    bezier_knot_acks_bmap_init(&ps->bezier_knot_acks);
    //food_acks_hm_init(&ps->food_acks);
}

void proximity_state_deinit(struct proximity_state* ps)
{
    //food_acks_hm_deinit(&ps->food_acks);
    bezier_knot_acks_bmap_deinit(ps->bezier_knot_acks);
}
