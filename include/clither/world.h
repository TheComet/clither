#pragma once

#include "cstructures/vector.h"

struct world
{
    struct cs_vector snakes;
};

void
world_init(struct world* w);

void
world_deinit(struct world* w);

#define WORLD_FOR_EACH_SNAKE(world, var) \
    VECTOR_FOR_EACH(&(world)->snakes, struct snake, var)

#define WORLD_END_EACH \
    VECTOR_END_EACH
