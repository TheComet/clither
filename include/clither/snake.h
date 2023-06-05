#pragma once

#include "clither/config.h"
#include "clither/controls.h"
#include "clither/snake_head.h"

#include "cstructures/vector.h"

C_BEGIN

struct snake_data
{
    char* name;

    /*
     * We keep a local list of points that are drawn out over time as the head
     * moves. These points are used to fit a bezier curve to the front part of
     * the snake.
     * 
     * Due to roll back requirements, this is a list of list of points. We keep
     * a history of points from previous fits in case we have to restore to
     * a previous state. Lists are removed as the snake's head position is ACK'd.
     */
    struct cs_vector points;  /* struct cs_vector of struct qwpos */

    /*
     * List of bezier handles that define the shape of the entire snake.
     */
    struct cs_vector bezier_handles;

    /*
     * The curve is sampled N times (N = length of snake) and the results are
     * cached here. These are used for rendering.
     */
    struct cs_vector bezier_points;

};

struct snake
{
    struct controls_rb controls_rb;
    struct snake_data data;
    struct snake_head head;
    struct snake_head head_ack;
};

void
snake_init(struct snake* snake, struct qwpos spawn_pos, const char* name);

void
snake_deinit(struct snake* snake);

void
snake_head_init(struct snake_head* head, struct qwpos spawn_pos);

void
snake_step_head(struct snake_head* head, const struct controls* controls, uint8_t sim_tick_rate);

void
snake_step(
    struct snake_data* data,
    struct snake_head* head,
    const struct controls* controls,
    uint8_t sim_tick_rate);

void
snake_ack_frame(
    struct snake_data* data,
    struct snake_head* acknowledged_head,
    struct snake_head* predicted_head,
    const struct snake_head* authoritative_head,
    struct controls_rb* controls_rb,
    uint16_t frame_number,
    uint8_t sim_tick_rate);

#define snake_length(snake) (vector_count(&(snake)->bezier_points))

C_END
