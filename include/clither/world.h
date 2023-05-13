#pragma once

#include "cstructures/btree.h"

struct world
{
    struct cs_btree snakes;
};

void
world_init(struct world* w);

void
world_deinit(struct world* w);

void
world_step(struct world* w, int sim_tick_rate);

#define WORLD_FOR_EACH_SNAKE(world, uid, snake) \
    BTREE_FOR_EACH(&(world)->snakes, struct snake, uid, snake)

#define WORLD_END_EACH \
    BTREE_END_EACH
