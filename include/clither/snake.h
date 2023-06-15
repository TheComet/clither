#pragma once

#include "clither/config.h"
#include "clither/command.h"
#include "clither/snake_head.h"

#include "cstructures/rb.h"
#include "cstructures/vector.h"

C_BEGIN

struct snake_param
{
    qa turn_speed;
    qw min_speed;
    qw max_speed;
    qw boost_speed;
    uint8_t acceleration;
};

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
    struct cs_rb points_lists;  /* struct cs_vector of struct qwpos */

    /*
     * List of bezier handles that define the shape of the entire snake.
     */
    struct cs_rb bezier_handles;

    /*
     * The curve is sampled N times (N = length of snake) and the results are
     * cached here. These are used for rendering.
     */
    struct cs_vector bezier_points;
};

struct snake
{
    struct command_rb command_rb;
    struct snake_param param;
    struct snake_data data;
    struct snake_head head;
    struct snake_head head_ack;

    unsigned hold : 1;
};

void
snake_init(struct snake* snake, struct qwpos spawn_pos, const char* name);

void
snake_deinit(struct snake* snake);

void
snake_param_init(struct snake_param* param);

void
snake_head_init(struct snake_head* head, struct qwpos spawn_pos);

void
snake_step_head(
    struct snake_head* head,
    const struct snake_param* param,
    struct command command,
    uint8_t sim_tick_rate);

void
snake_step(
    struct snake_data* data,
    struct snake_head* head,
    const struct snake_param* param,
    struct command command,
    uint8_t sim_tick_rate);

void
snake_ack_frame(
    struct snake_data* data,
    struct snake_head* acknowledged_head,
    struct snake_head* predicted_head,
    const struct snake_head* authoritative_head,
    const struct snake_param* param,
    struct command_rb* command_rb,
    uint16_t frame_number,
    uint8_t sim_tick_rate);

/*
 * \brief Holds the snake in-place and doesn't simulate. Only
 * relevant for the server.
 */
#define snake_set_hold(snake) \
    (snake)->hold = 1

/*!
 * \brief If a snake is in hold mode (@see snake_set_hold), this
 * function checks the condition for resetting the hold state.
 * If the condition applies, the snake's hold state is reset.
 */
char
snake_is_held(struct snake* snake, uint16_t frame_number);

#define snake_length(snake) (vector_count(&(snake)->bezier_points))

C_END
