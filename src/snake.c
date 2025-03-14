#include "clither/bezier.h"
#include "clither/bezier_handle_rb.h"
#include "clither/bezier_point_vec.h"
#include "clither/log.h"
#include "clither/q.h"
#include "clither/qwaabb_rb.h"
#include "clither/qwpos_vec.h"
#include "clither/qwpos_vec_rb.h"
#include "clither/snake.h"
#include "clither/str.h"
#include "clither/wrap.h"

#define _USE_MATH_DEFINES
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SNAKE_PART_SPACING make_qw2(1, 6)

/* ------------------------------------------------------------------------- */
void snake_head_init(struct snake_head* head, struct qwpos spawn_pos)
{
    head->pos = spawn_pos;
    head->angle = make_qa(0);
    head->speed = 0;
}

/* ------------------------------------------------------------------------- */
static int snake_data_init(
    struct snake_data* data, struct qwpos spawn_pos, const char* name)
{
    struct qwpos_vec**    trail;
    struct bezier_handle* h1;
    struct bezier_handle* h2;
    struct qwaabb*        aabb;

    str_init(&data->name);
    if (str_set_cstr(&data->name, name) != 0)
        goto set_name_failed;

    qwpos_vec_rb_init(&data->head_trails);
    bezier_handle_rb_init(&data->bezier_handles);
    qwaabb_rb_init(&data->bezier_aabbs);
    bezier_point_vec_init(&data->bezier_points);

    /*
     * Create the initial trail, which is the list of points the curve
     * is fitted to. This grows as the head moves forwards. Add the spawn pos
     * now, as we want the curve to begin there.
     */
    trail = qwpos_vec_rb_emplace_realloc(&data->head_trails);
    if (trail == NULL)
        goto emplace_trail_failed;
    qwpos_vec_init(trail);
    if (qwpos_vec_push(trail, spawn_pos) != 0)
        goto push_spawn_pos_failed;

    /*
     * Create the first bezier segment, which consists of two handles. By
     * convention all snakes start out facing to the right, but maybe this can
     * be changed in the future.
     */
    h1 = bezier_handle_rb_emplace_realloc(&data->bezier_handles);
    if (h1 == NULL)
        goto emplace_h1_failed;
    bezier_handle_init(h1, spawn_pos, make_qa(0));
    h2 = bezier_handle_rb_emplace_realloc(&data->bezier_handles);
    if (h2 == NULL)
        goto emplace_h2_failed;
    bezier_handle_init(h2, spawn_pos, make_qa(0));

    /*
     * Create the curve's bounding box and also set the entire snake's bounding
     * box.
     */
    aabb = qwaabb_rb_emplace_realloc(&data->bezier_aabbs);
    if (aabb == NULL)
        goto emplace_aabb_failed;
    data->aabb = *aabb =
        make_qwaabbqw(spawn_pos.x, spawn_pos.y, spawn_pos.x, spawn_pos.y);

    return 0;

emplace_aabb_failed:
emplace_h2_failed:
emplace_h1_failed:
push_spawn_pos_failed:
    while (rb_count(data->head_trails) > 0)
        qwpos_vec_deinit(qwpos_vec_rb_take(data->head_trails));
emplace_trail_failed:
    bezier_point_vec_deinit(data->bezier_points);
    qwaabb_rb_deinit(data->bezier_aabbs);
    bezier_handle_rb_deinit(data->bezier_handles);
    qwpos_vec_rb_deinit(data->head_trails);
    str_deinit(data->name);
set_name_failed:
    return -1;
}

/* ------------------------------------------------------------------------- */
static void snake_data_deinit(struct snake_data* data)
{
    bezier_point_vec_deinit(data->bezier_points);
    qwaabb_rb_deinit(data->bezier_aabbs);
    bezier_handle_rb_deinit(data->bezier_handles);
    while (rb_count(data->head_trails) > 0)
        qwpos_vec_deinit(qwpos_vec_rb_take(data->head_trails));
    qwpos_vec_rb_deinit(data->head_trails);
    str_deinit(data->name);
}

/* ------------------------------------------------------------------------- */
int snake_init(struct snake* snake, struct qwpos spawn_pos, const char* name)
{
    if (snake_data_init(&snake->data, spawn_pos, name) != 0)
        return -1;
    cmd_queue_init(&snake->cmdq);
    snake_param_init(&snake->param);
    snake_head_init(&snake->head, spawn_pos);
    snake_head_init(&snake->head_ack, spawn_pos);

    snake->hold = 0;
    return 0;
}

/* ------------------------------------------------------------------------- */
void snake_deinit(struct snake* snake)
{
    snake_data_deinit(&snake->data);
    cmd_queue_deinit(&snake->cmdq);
}

/* ------------------------------------------------------------------------- */
void snake_step_head(
    struct snake_head*        head,
    const struct snake_param* param,
    struct cmd                command,
    uint8_t                   sim_tick_rate)
{
    qw      dx, dy;
    uint8_t target_speed;

    /*
     * The command structure contains the absolute angle (in world space) of
     * the desired angle. That is, the angle from the snake's head to the mouse
     * cursor. It is stored in an unsigned char [0 .. 255]. We need to convert
     * it to radians [-pi .. pi) using the fixed point angle type "qa"
     */
    qa target_angle = u8_to_qa(command.angle);

    /* Calculate difference between desired angle and actual angle and wrap */
    qa angle_diff = qa_sub(head->angle, target_angle);

    /*
     * Turn the head towards the target angle and make sure to not exceed the
     * maximum turning speed.
     */
    if (angle_diff > snake_turn_speed(param))
        head->angle = qa_sub(
            head->angle,
            qa_mul(snake_turn_speed(param), make_qa2(sim_tick_rate, 60)));
    else if (angle_diff < -snake_turn_speed(param))
        head->angle = qa_add(head->angle, snake_turn_speed(param));
    else
        head->angle = target_angle;

    /* Integrate speed over time */
    target_speed =
        command.action == CMD_ACTION_BOOST
            ? 255
            : qw_sub(snake_max_speed(param), snake_min_speed(param)) *
                  command.speed /
                  qw_sub(snake_boost_speed(param), snake_min_speed(param));
    if (head->speed - target_speed > snake_acceleration(param))
        head->speed -= snake_acceleration(param);
    else if (head->speed - target_speed < -snake_acceleration(param))
        head->speed += snake_acceleration(param);
    else
        head->speed = target_speed;

    /* Update snake position using the head's current angle and speed */
    dx = qw_sub(snake_boost_speed(param), snake_min_speed(param));
    dx = qw_rescale(dx, head->speed, 255);
    dx = qw_add(dx, snake_min_speed(param));
    dx = qw_mul(qa_cos(head->angle), dx);
    head->pos.x = qw_add(head->pos.x, dx);

    dy = qw_sub(snake_boost_speed(param), snake_min_speed(param));
    dy = qw_rescale(dy, head->speed, 255);
    dy = qw_add(dy, snake_min_speed(param));
    dy = qw_mul(qa_sin(head->angle), dy);
    head->pos.y = qw_add(head->pos.y, dy);
}

/* ------------------------------------------------------------------------- */
/*!
 * \brief Recalculates the AABB of the entire curve/snake by merging the AABBs
 * of each segment.
 */
static void snake_update_aabb(struct snake_data* data)
{
    int i;
    data->aabb = *rb_peek(data->bezier_aabbs, 0);
    for (i = 1; i < rb_count(data->bezier_aabbs); ++i)
    {
        struct qwaabb aabb = *rb_peek(data->bezier_aabbs, i);
        data->aabb = qwaabb_union(data->aabb, aabb);
    }
}

/* ------------------------------------------------------------------------- */
/*!
 * \brief Updates the front-most segment of the curve (the head).
 */
static void snake_update_head_trail_aabb(struct snake_data* data)
{
    int                     i;
    struct qwaabb*          bb = rb_peek_write(data->bezier_aabbs);
    const struct qwpos_vec* trail = *rb_peek_write(data->head_trails);
    const struct qwpos*     p = vec_get(trail, 0);

    /*
     * The AABB *has* to be calculated from the trail, rather than from the
     * bezier curve. If we calc it from the curve, then the aabb may be slightly
     * smaller than the area spanned by the original points in the trail due to
     * the fit error, making a weird edge case possible where the acknowledged
     * head position can end up outside of the bounding box. If this happens,
     * combined with large latency, it's possible a segment required for
     * rollback is removed from the snake, leading to a crash.
     *
     * In short: DON'T use bezier_calc_aabb() here.
     */
    *bb = make_qwaabbqw(p->x, p->y, p->x, p->y);
    for (i = 1; i < vec_count(trail); ++i)
    {
        p = vec_get(trail, i);
        if (bb->x1 > p->x)
            bb->x1 = p->x;
        if (bb->x2 < p->x)
            bb->x2 = p->x;
        if (bb->y1 > p->y)
            bb->y1 = p->y;
        if (bb->y2 < p->y)
            bb->y2 = p->y;
    }
}

/* ------------------------------------------------------------------------- */
static int snake_update_curve_from_head(
    struct snake_data* data, const struct snake_head* head)
{
    struct qwpos_vec** trail;
    double             error_squared;

    /* Append new position to the trail */
    trail = rb_peek_write(data->head_trails);
    qwpos_vec_push(trail, head->pos);

    /* Fit current bezier segment to trail */
    error_squared = bezier_fit_trail(
        rb_peek(data->bezier_handles, rb_count(data->bezier_handles) - 1),
        rb_peek(data->bezier_handles, rb_count(data->bezier_handles) - 2),
        *trail);

    /*
     * If the fit's error exceeds some threshold (determined empirically),
     * signal that a new segment needs to be created.
     */
    return error_squared > make_q16_16_2(1, 16);
}

/* ------------------------------------------------------------------------- */
static void
snake_add_new_segment(struct snake_data* data, const struct snake_head* head)
{
    /*
     * Create new trail, which is the list of points the curve is fitted
     * to. This grows as the head moves forwards. Add the current head position
     * now, because we will want the start position of the curve to line up
     * with the end position of the previous curve.
     */
    struct qwpos_vec** trail = qwpos_vec_rb_emplace_realloc(&data->head_trails);
    qwpos_vec_init(trail);
    qwpos_vec_push(trail, head->pos);

    /*
     * Add a new bezier handle. Since there is only one datapoint, the curve
     * is completely defined by the current head position.
     */
    bezier_handle_init(
        bezier_handle_rb_emplace_realloc(&data->bezier_handles),
        head->pos,
        qa_add(head->angle, QA_PI));

    /* Add a new bounding box, which is also defined by the current head
     * position */
    *qwaabb_rb_emplace_realloc(&data->bezier_aabbs) =
        make_qwaabbqw(head->pos.x, head->pos.y, head->pos.x, head->pos.y);
}

/* ------------------------------------------------------------------------- */
int snake_step(
    struct snake_data*        data,
    struct snake_head*        head,
    const struct snake_param* param,
    struct cmd                command,
    uint8_t                   sim_tick_rate)
{
    int need_new_segment;

    snake_step_head(head, param, command, sim_tick_rate);
    need_new_segment = snake_update_curve_from_head(data, head);

    /*
     * Have to call these after updating curve data, because only then is the
     * point trail updated (and this is required for AABBs)
     */
    snake_update_head_trail_aabb(data);
    snake_update_aabb(data);

    if (need_new_segment)
        snake_add_new_segment(data, head);

    bezier_squeeze_step(data->bezier_handles, sim_tick_rate);

    /* This function returns the number of segments that are superfluous. */
    return bezier_calc_equidistant_points(
        &data->bezier_points,
        data->bezier_handles,
        qw_mul(SNAKE_PART_SPACING, snake_scale(param)),
        snake_length(param));
}

/* ------------------------------------------------------------------------- */
void snake_remove_stale_segments(struct snake_data* data, int stale_segments)
{
    CLITHER_DEBUG_ASSERT(stale_segments < rb_count(data->head_trails));

    while (stale_segments--)
    {
        qwpos_vec_deinit(qwpos_vec_rb_take(data->head_trails));
        bezier_handle_rb_take(data->bezier_handles);
        qwaabb_rb_take(data->bezier_aabbs);
    }

    snake_update_aabb(data);
}

/* ------------------------------------------------------------------------- */
void snake_remove_stale_segments_with_rollback_constraint(
    struct snake_data*       data,
    const struct snake_head* head_ack,
    int                      stale_segments)
{
    assert(stale_segments < rb_count(data->head_trails));

    while (stale_segments--)
    {
        /*
         * If at any point the acknowledged head position is on a curve segment
         * that we want to remove, abort, because this curve segment is still
         * required for rollback.
         */
        if (qwaabb_test_qwpos(*rb_peek_read(data->bezier_aabbs), head_ack->pos))
            break;

        qwpos_vec_deinit(qwpos_vec_rb_take(data->head_trails));
        bezier_handle_rb_take(data->bezier_handles);
        qwaabb_rb_take(data->bezier_aabbs);
    }

    snake_update_aabb(data);
}

/* ------------------------------------------------------------------------- */
void snake_ack_frame(
    struct snake_data*        data,
    struct snake_head*        acknowledged_head,
    struct snake_head*        predicted_head,
    const struct snake_head*  authoritative_head,
    const struct snake_param* param,
    struct cmd_queue*         cmdq,
    uint16_t                  frame_number,
    uint8_t                   sim_tick_rate)
{
    uint16_t last_ackd_frame, predicted_frame;

    if (cmd_queue_count(cmdq) == 0)
    {
        log_warn(
            "snake_ack_frame(): Command buffer of snake \"%s\" is empty. Can't "
            "step.\n",
            str_cstr(data->name));
        return;
    }
    last_ackd_frame = cmd_queue_frame_begin(cmdq);
    predicted_frame = cmd_queue_frame_end(cmdq);

    /* last_ackd_frame <= frame_number <= predicted_frame */
    if (u16_lt_wrap(frame_number, last_ackd_frame) ||
        u16_gt_wrap(frame_number, predicted_frame))
    {
        log_warn(
            "snake_ack_frame(): Frame number is outside of the command buffer "
            "range! Something is very wrong.\n"
            "  frame_number=%d\n"
            "  last_ackd_frame=%d\n"
            "  predicted_frame=%d\n",
            frame_number,
            last_ackd_frame,
            predicted_frame);
        return;
    }

    /*
     * It's possible the authoritative head position we receive from the server
     * goes through some packet loss, so may have to catch up.
     */
    while (u16_le_wrap(last_ackd_frame, frame_number))
    {
        /* "last_ackd_frame" refers to the next frame to simulate on the ack'd
         * head */
        struct cmd command = cmd_queue_take_or_predict(cmdq, last_ackd_frame);
        snake_step_head(acknowledged_head, param, command, sim_tick_rate);
        last_ackd_frame++;
    }

    /*
     * Our simulation of the last acknowledged head position diverges from the
     * server's head position. This means the predicted head position is also
     * incorrect.
     */
    if (snake_heads_are_equal(acknowledged_head, authoritative_head) == 0)
    {
        int               handles_to_squeeze;
        struct qwpos_vec* trail;
        uint16_t          frame;
        int               i;
        struct cmd*       command;

        log_dbg(
            "Rollback from frame %d to %d\n"
            "  ackd head: pos=%d,%d, angle=%d, speed=%d\n"
            "  auth head: pos=%d,%d, angle=%d, speed=%d\n",
            predicted_frame,
            frame_number,
            acknowledged_head->pos.x,
            acknowledged_head->pos.y,
            acknowledged_head->angle,
            acknowledged_head->speed,
            authoritative_head->pos.x,
            authoritative_head->pos.y,
            authoritative_head->angle,
            authoritative_head->speed);

        /*
         * Remove all points generated since the last acknowledged frame.
         * In rare cases this may be on the boundary of two bezier segments,
         * in which case we must also remove the segment and corresponding
         * trail.
         *
         * Also note that the first and last points in two adjacent trails
         * share the same position, so when removing a bezier segment, two
         * points need to be removed.
         */
        assert(rb_count(data->head_trails) > 0);
        trail = *rb_peek_write(data->head_trails);
        while (u16_gt_wrap(predicted_frame, frame_number))
        {
            qwpos_vec_pop(trail);
            if (vec_count(trail) == 0)
            {
                qwpos_vec_deinit(qwpos_vec_rb_takew(data->head_trails));
                bezier_handle_rb_takew(data->bezier_handles);
                qwaabb_rb_takew(data->bezier_aabbs);

                /* Remove duplicate point */
                trail = *rb_peek_write(data->head_trails);
                qwpos_vec_pop(trail);
            }

            predicted_frame--;
        }

        /*
         * Restore head positions to authoritative state, which counts as the
         * first "step" forwards
         */
        *acknowledged_head = *authoritative_head;
        *predicted_head = *authoritative_head;
        handles_to_squeeze = 0;
        if (snake_update_curve_from_head(data, predicted_head))
        {
            snake_update_head_trail_aabb(data);
            snake_add_new_segment(data, predicted_head);
            handles_to_squeeze++;
        }

        /* Simulate head forwards again */
        cmd_queue_for_each(cmdq, i, frame, command)
        {
            snake_step_head(predicted_head, param, *command, sim_tick_rate);
            if (snake_update_curve_from_head(data, predicted_head))
            {
                snake_update_head_trail_aabb(data);
                snake_add_new_segment(data, predicted_head);
                handles_to_squeeze++;
            }

            /*
             * The snake's bezier handles are "squeezed" over time. Only have
             * to re-squeeze the handles that were recreated during forwards
             * simulation.
             */
            bezier_squeeze_n_recent_step(
                data->bezier_handles, handles_to_squeeze, sim_tick_rate);
        }

        snake_update_head_trail_aabb(data);
        snake_update_aabb(data);

        /* TODO: distance is a function of the snake's length */
        bezier_calc_equidistant_points(
            &data->bezier_points,
            data->bezier_handles,
            qw_mul(SNAKE_PART_SPACING, snake_scale(param)),
            snake_length(param));
    }
}

/* ------------------------------------------------------------------------- */
int snake_try_reset_hold(struct snake* snake, uint16_t frame_number)
{
    if (cmd_queue_count(&snake->cmdq) > 0 &&
        cmd_queue_frame_begin(&snake->cmdq) == frame_number)
    {
        snake->hold = 0;
    }

    return !snake->hold;
}
