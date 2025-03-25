#pragma once

#include "clither/hm.h"
#include "clither/q.h"
#include "clither/bset.h"

struct food
{
    struct qwpos pos;
};

BSET_DECLARE(food_bset, uint64_t, 32)

struct food_grid
{
    struct food_bset* morton;
};

void food_grid_init(struct food_grid* grid);
void food_grid_deinit(struct food_grid* grid);

int food_grid_add_food(struct food_grid* grid, struct qwpos pos);
int food_grid_remove_food(struct food_grid* grid, struct qwpos pos);

static int food_grid_food_count(const struct food_grid* grid)
{
    return bset_count(grid->morton);
}
