#pragma once

#include "clither/config.h"
#include "clither/q.h"

#include "cstructures/btree.h"

C_BEGIN

struct world
{
    struct cs_btree snakes;
    qw inner_radius;
    qw ring_start;
    qw ring_end;
};

void
world_init(struct world* world);

void
world_deinit(struct world* world);

struct snake*
world_create_snake(struct world* world, uint16_t snake_id, struct qwpos spawn_pos, const char* username);

uint16_t
world_spawn_snake(struct world* world, const char* username);

#define world_get_snake(world, snake_id) \
    ((struct snake*)btree_find(&(world)->snakes, snake_id))

void
world_remove_snake(struct world* world, uint16_t snake_id);

void
world_step(struct world* w, uint16_t frame_number, uint8_t sim_tick_rate);

#define WORLD_FOR_EACH_SNAKE(world, uid, snake) \
    BTREE_FOR_EACH(&(world)->snakes, struct snake, uid, snake)

#define WORLD_END_EACH \
    BTREE_END_EACH

C_END
