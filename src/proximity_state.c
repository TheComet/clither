#include "clither/bezier_pending_acks_bset.h"
#include "clither/proximity_state.h"

void proximity_state_init(struct proximity_state* ps)
{
    bezier_pending_acks_bset_init(&ps->bezier_pending_acks);
}

void proximity_state_deinit(struct proximity_state* ps)
{
    bezier_pending_acks_bset_deinit(ps->bezier_pending_acks);
}
