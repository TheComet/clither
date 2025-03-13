#pragma once

#include "clither/bezier.h"
#include "clither/cmd_queue.h"
#include "clither/snake_param.h"

struct snake_head
{
    struct qwpos pos;
    qa           angle;
    uint8_t      speed;
};

struct snake_split
{
    struct qwpos split_handle; /* Set to equal the position of a bezier handle
                                  where the snake splits into 2 */
    struct qwpos join_handle;  /* Set to equal the position of a bezier handle
                                  where the snake joins up again */
    struct qwpos axis; /* Snake is mirrored across this normalized vector */
    unsigned ack : 1; /* Server acknowledged this split. Predicted splits can be
                         removed during resimulation */
};

struct snake_data
{
    /* Username stored here */
    struct str* name;

    /*
     * We keep a local trail of points that are drawn out over time as the head
     * moves. These points are used to fit a bezier curve to the front part of
     * the snake.
     *
     * Due to roll back requirements, this is a list of list of vec2's. We keep
     * a history of points from previous fits in case we have to restore to
     * a previous state. Lists are removed as the snake's head position is
     * ACK'd.
     */
    struct qwpos_vec_rb* head_trails;

    /* List of bezier handles that define the shape of the entire snake. */
    struct bezier_handle_rb* bezier_handles;

    /*
     * List of axis-aligned bounding-boxes (qwaabb) for each bezier segment.
     * This list will be 1 shorter than the list of bezier_handles.
     */
    struct qwaabb_rb* bezier_aabbs;

    /*
     * The curve is sampled N times (N = length of snake) and the results are
     * cached here. These are used for rendering.
     */
    struct bezier_point_vec* bezier_points;

    struct snake_splits_rb* splits;

    /* AABB of the entire snake */
    struct qwaabb aabb;
};

struct snake
{
    struct cmd_queue   cmdq;
    struct snake_param param;
    struct snake_data  data;
    struct snake_head  head;
    struct snake_head  head_ack;

    unsigned hold : 1;
};

int snake_init(struct snake* snake, struct qwpos spawn_pos, const char* name);

void snake_deinit(struct snake* snake);

void snake_head_init(struct snake_head* head, struct qwpos spawn_pos);

static inline int
snake_heads_are_equal(const struct snake_head* a, const struct snake_head* b)
{
    return a->pos.x == b->pos.x && a->pos.y == b->pos.y &&
           a->angle == b->angle && a->speed == b->speed;
}

/*
 * \brief Holds the snake in-place and doesn't simulate. Only
 * relevant for the server.
 */
#define snake_set_hold(snake) (snake)->hold = 1

#define snake_is_held(snake) (snake)->hold

/*!
 * \brief If a snake is in hold mode (@see snake_set_hold), this
 * function checks the condition for resetting the hold state.
 * If the condition applies, the snake's hold state is reset and
 * true is returned. Otherwise false is returned.
 */
int snake_try_reset_hold(struct snake* snake, uint16_t frame_number);

void snake_step_param(
    struct snake_data* data, struct snake_param* param, uint32_t food_eaten);

char snake_is_split(const struct snake_data* data);

void snake_step_head(
    struct snake_head*        head,
    const struct snake_param* param,
    struct cmd                command,
    uint8_t                   sim_tick_rate);

/*!
 * \brief Steps the snake forward by 1 frame, using the given command.
 * \param[in] data The snake's data structure. Trails and curves are
 * updated accordingly.
 * \param[in] head The snake's head is moved forwards by calling
 * snake_step_head().
 * \param[in] param The matching parameters for the snake's head. The params
 * can change every frame so a history is maintained for rollback purposes. This
 * is currently not implemented.
 * \param[in] command The command to step forwards with.
 * \param[in] sim_tick_rate The simulation speed.
 * \return Returns the number of segments that could be removed from the curve.
 *
 * On the server-side this value should be passed to a proceeding call to
 * snake_remove_stale_segments().
 *
 * On the client-side, this is handled by snake_ack_frame() instead.
 */
int snake_step(
    struct snake_data*        data,
    struct snake_head*        head,
    const struct snake_param* param,
    struct cmd                command,
    uint8_t                   sim_tick_rate);

void snake_remove_stale_segments(struct snake_data* data, int stale_segments);

void snake_remove_stale_segments_with_rollback_constraint(
    struct snake_data*       data,
    const struct snake_head* head_ack,
    int                      stale_segments);

void snake_ack_frame(
    struct snake_data*        data,
    struct snake_head*        acknowledged_head,
    struct snake_head*        predicted_head,
    const struct snake_head*  authoritative_head,
    const struct snake_param* param,
    struct cmd_queue*         cmdq,
    uint16_t                  frame_number,
    uint8_t                   sim_tick_rate);
