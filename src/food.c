#include "clither/food.h"
#include "clither/morton.h"

BSET_DEFINE(food_bset, uint64_t, 32)

void food_grid_init(struct food_grid* grid)
{
    food_bset_init(&grid->morton);
}

void food_grid_deinit(struct food_grid* grid)
{
    food_bset_deinit(grid->morton);
}

int food_grid_add_food(struct food_grid* grid, struct qwpos pos)
{
    uint64_t m = morton_encode_qwpos(pos);
    switch (food_bset_insert(&grid->morton, m))
    {
        case BSET_OOM: return -1;
        case BSET_EXISTS: break;
        case BSET_NEW: break;
    }
    return 0;
}

int food_grid_remove_food(struct food_grid* grid, struct qwpos pos)
{
    uint64_t m = morton_encode_qwpos(pos);
    return food_bset_erase(grid->morton, m);
}
