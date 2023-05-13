#include "clither/snake.h"
#include "clither/world.h"

#include <stddef.h>

/* ------------------------------------------------------------------------- */
void
world_init(struct world* w)
{
    btree_init(&w->snakes, sizeof(struct snake));
}

/* ------------------------------------------------------------------------- */
void
world_deinit(struct world* w)
{
    WORLD_FOR_EACH_SNAKE(w, uid, snake)
        snake_deinit(snake);
    WORLD_END_EACH
    btree_deinit(&w->snakes);
}

/* ------------------------------------------------------------------------- */
void
world_step(struct world* w, int sim_tick_rate)
{
    WORLD_FOR_EACH_SNAKE(w, uid, snake)
        snake_step(snake, sim_tick_rate);
    WORLD_END_EACH
}
