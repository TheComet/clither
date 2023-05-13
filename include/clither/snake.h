#pragma once

#include "clither/config.h"
#include "clither/controls.h"
#include "clither/log.h"

#include "cstructures/vector.h"

#include <math.h>

C_BEGIN

struct controls;

struct snake
{
    char* name;
    struct controls controls;

    /*
     * We keep a local list of points that are drawn out over time as the head
     * moves. These points are used to fit a bezier curve to the front part of
     * the snake, so the list of points will be as long as the first section of
     * the snake.
     */
    struct cs_vector points;

    /*
     * List of bezier handles that completely define the snake. Each handle is
     * its own network object.
     */
    struct cs_vector bezier_handles;

    /*
     * The total length (in points) of the snake. The list of bezier handles
     * should always span a distance that is longer than the total length of
     * the snake.
     */
    uint32_t length;
};

void
snake_init(struct snake* s, const char* name);

void
snake_deinit(struct snake* s);

void
snake_update_controls(struct snake* s, const struct input* i);

void
snake_step(struct snake* s, int sim_tick_rate);

C_END
