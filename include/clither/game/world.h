#pragma once

#include "clither/game/food.h"
#include "clither/game/q.h"

struct snake_bmap;
struct snake_data;
struct snake_param;
struct food_grid;
struct settings_world;

struct world
{
    struct snake_bmap* snakes;
    struct food_grid   food_grid;
    uint32_t           rng;
    int                food_count;
    qw                 inner_radius;
    qw                 ring_start;
    qw                 ring_end;
    uint16_t           next_free_snake_id;
};

void world_init(struct world* world);

void world_deinit(struct world* world);

void world_update_settings(
    struct world* world, const struct settings_world* settings);

/*
 * \brief Spawn a new snake in the world at a random location and return the
 * snake ID. This is usually a server-side call.
 * \return The snake ID of the newly spawned snake. If the function fails, 0 is
 * returned.
 */
uint16_t world_spawn_snake(struct world* world, const char* username);

/*!
 * \brief Same as world_spawn_snake(), except the spawn position and snake ID
 * are parameters instead of being determined automatically. This is usually
 * a client-side call.
 */
struct snake* world_create_snake(
    struct world* world,
    uint16_t      snake_id,
    struct qwpos  spawn_pos,
    const char*   username);

void world_remove_snake(struct world* world, uint16_t snake_id);

int world_respawn_food(struct world* w);
int world_spawn_food_corpse(
    struct world*             w,
    const struct snake_data*  data,
    const struct snake_param* param);
