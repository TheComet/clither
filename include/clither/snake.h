#pragma once

#include "clither/config.h"
#include "clither/controls.h"
#include "clither/q.h"

#include "cstructures/vector.h"

C_BEGIN

struct controls;

struct snake
{
    char* name;
    struct controls controls;

    struct qwpos2 head_pos;
    qa head_angle;
    uint8_t speed;

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
snake_init(struct snake* snake, const char* name);

void
snake_deinit(struct snake* snake);

void
snake_update_controls(struct snake* snake, const struct controls* controls);

void
snake_step(struct snake* snake, int sim_tick_rate);

C_END
