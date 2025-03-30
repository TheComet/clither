#pragma once

#include "clither/game/q.h"
#include "clither/util/bmap.h"

struct food
{
    /* Direction vector (normalized). Used for rotating the sprite */
    struct qwpos dir;
    /* How much the snake grows when it eats this food */
    uint8_t value;
};

BMAP_DECLARE(food_bmap, uint64_t, struct food, 32)

struct food_grid
{
    struct food_bmap* morton;
};

void food_grid_init(struct food_grid* grid);
void food_grid_deinit(struct food_grid* grid);

int food_grid_add_food(
    struct food_grid* grid, struct qwpos pos, struct qwpos dir);
int food_grid_remove_food(struct food_grid* grid, uint64_t morton);

static int food_grid_food_count(const struct food_grid* grid)
{
    return bmap_count(grid->morton);
}
