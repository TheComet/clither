#pragma once

#include "clither/config.h"
#include "clither/snake_head.h"

#include "cstructures/vector.h"

C_BEGIN

struct controls;

struct snake
{
    char* name;

    struct cs_vector controls_buffer;

    /*
     * We keep a local list of points that are drawn out over time as the head
     * moves. These points are used to fit a bezier curve to the front part of
     * the snake, so the list of points will be as long as the first section of
     * the snake.
     */
    struct cs_vector points;

    /*
     * List of bezier handles that define the shape of the entire snake.
     */
    struct cs_vector bezier_handles;

    /*
     * The entire curve (all bezier segments) is sampled N number of times,
     * where N is the length of the snake.
     */
    struct cs_vector bezier_points;
    struct cs_vector bezier_points_ackd;

    /*
     * The total length (in points) of the snake. The list of bezier handles
     * should always span a distance that is longer than the total length of
     * the snake.
     */
    uint32_t length;

    struct snake_head head;
    struct snake_head head_ackd;
};

void
snake_init(struct snake* snake, struct qwpos spawn_pos, const char* name);

void
snake_deinit(struct snake* snake);

void
snake_queue_controls(struct snake* snake, const struct controls* controls, uint16_t frame_number);

void
snake_ack_frame(struct snake* snake, const struct snake_head* auth, uint16_t frame_number, uint8_t sim_tick_rate);

#define snake_controls_count(snake) \
    (btree_count(&snake->controls_buffer))

void
snake_step_head(struct snake_head* head, const struct controls* controls, uint8_t sim_tick_rate);

void
snake_step(struct snake* snake, uint16_t frame_number, uint8_t sim_tick_rate);

C_END
