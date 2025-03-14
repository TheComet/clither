#pragma once

struct bezier_pending_acks_set;

struct proximity_state
{
    struct bezier_pending_acks_bset* bezier_pending_acks;
};

void proximity_state_init(struct proximity_state* ps);
void proximity_state_deinit(struct proximity_state* ps);
