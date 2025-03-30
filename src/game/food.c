#include "clither/game/food.h"
#include "clither/util/morton.h"

BMAP_DEFINE(food_bmap, uint64_t, struct food, 32)

void food_grid_init(struct food_grid* grid)
{
    food_bmap_init(&grid->morton);
}

void food_grid_deinit(struct food_grid* grid)
{
    food_bmap_deinit(grid->morton);
}

int food_grid_add_food(
    struct food_grid* grid, struct qwpos pos, struct qwpos dir)
{
    uint64_t     m = morton_encode_qwpos(pos);
    struct food* new_food;
    switch (food_bmap_emplace_new(&grid->morton, m, &new_food))
    {
        case BMAP_OOM: return -1;
        case BMAP_NEW: new_food->dir = dir; new_food->value = 5;
        case BMAP_EXISTS: break;
    }
    return 0;
}

int food_grid_remove_food(struct food_grid* grid, uint64_t morton)
{
    return food_bmap_erase(grid->morton, morton);
}
