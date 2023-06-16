#pragma once

#include "clither/q.h"

struct snake_head
{
    struct qwpos pos;
    qa angle;
    uint8_t speed;
    uint32_t food_eaten;
};

static inline int
snake_heads_are_equal(const struct snake_head* a, const struct snake_head* b)
{
    return a->pos.x == b->pos.x
        && a->pos.y == b->pos.y
        && a->angle == b->angle
        && a->speed == b->speed
        && a->food_eaten == b->food_eaten;
}
