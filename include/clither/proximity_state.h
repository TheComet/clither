#pragma once

struct bezier_knot_acks_bmap;
struct food_acks_hm;

struct proximity_state
{
    struct bezier_knot_acks_bmap* bezier_knot_acks;
};

void proximity_state_init(struct proximity_state* ps);
void proximity_state_deinit(struct proximity_state* ps);
