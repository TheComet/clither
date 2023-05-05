#include "clither/snake.h"
#include "clither/world.h"

/* ------------------------------------------------------------------------- */
void
world_init(struct world* w)
{
    vector_init(&w->snakes, sizeof(struct snake));
}

/* ------------------------------------------------------------------------- */
void
world_deinit(struct world* w)
{
    WORLD_FOR_EACH_SNAKE(w, snake)
        snake_deinit(snake);
    WORLD_END_EACH
    vector_deinit(&w->snakes);
}
