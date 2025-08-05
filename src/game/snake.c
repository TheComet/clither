#include "clither/game/bezier.h"
#include "clither/game/bezier_knot_rb.h"
#include "clither/game/bezier_segment_rb.h"
#include "clither/game/food.h"
#include "clither/game/q.h"
#include "clither/game/qwaabb_rb.h"
#include "clither/game/qwpos_vec.h"
#include "clither/game/qwpos_vec_rb.h"
#include "clither/game/snake.h"
#include "clither/game/snake_split_rb.h"
#include "clither/game/wrap.h"
#include "clither/util/log.h"
#include "clither/util/morton.h"
#include "clither/util/str.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
static int
add_new_segment(struct snake_data* data, struct qwpos pos, qa forward_angle)
{
    struct qwpos_vec**     trail;
    struct bezier_knot*    knot;
    struct bezier_segment* segment;
    struct qwaabb*         bb;

    /*
     * Create new trail, which is the list of points the curve is fitted
     * to. This grows as the head moves forwards. Add the current head position
     * now, because we will want the start position of the curve to line up
     * with the end position of the previous curve.
     */
    trail = qwpos_vec_rb_emplace_realloc(&data->trails);
    if (trail == NULL)
        goto emplace_trail_failed;
    qwpos_vec_init(trail);
    qwpos_vec_push(trail, pos);

    /* Add a new bezier knot. Since there is only one datapoint, the curve
     * is completely defined by the current head position. */
    knot = bezier_knot_rb_emplace_realloc(&data->knots);
    if (knot == NULL)
        goto emplace_knot_failed;
    bezier_knot_init(knot, pos, qa_add(forward_angle, QA_PI), 0, 0);

    /* Create a new segment using the new knot and previous knot */
    segment = bezier_segment_rb_emplace_realloc(&data->segments);
    if (segment == NULL)
        goto emplace_segment_failed;
    bezier_calc_segment(
        segment, knot, rb_peek(data->knots, rb_count(data->knots) - 2));

    /* Add a new bounding box */
    bb = qwaabb_rb_emplace_realloc(&data->segment_bbs);
    if (bb == NULL)
        goto emplace_bb_failed;
    *bb = make_qwaabbqw(pos.x, pos.y, pos.x, pos.y);

    return 0;

emplace_bb_failed:
    bezier_segment_rb_take(data->segments);
emplace_segment_failed:
    bezier_knot_rb_take(data->knots);
emplace_knot_failed:
    qwpos_vec_deinit(qwpos_vec_rb_take(data->trails));
emplace_trail_failed:
    return -1;
}

/* ------------------------------------------------------------------------- */
static void snake_remove_segment_from_tail(struct snake_data* data)
{
    qwpos_vec_deinit(qwpos_vec_rb_take(data->trails));
    bezier_knot_rb_take(data->knots);
    bezier_segment_rb_take(data->segments);
    qwaabb_rb_take(data->segment_bbs);
}

/* ------------------------------------------------------------------------- */
static void remove_segment_from_head(struct snake_data* data)
{
    qwpos_vec_deinit(qwpos_vec_rb_takew(data->trails));
    bezier_knot_rb_takew(data->knots);
    bezier_segment_rb_takew(data->segments);
    qwaabb_rb_takew(data->segment_bbs);
}

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
    struct bezier_knot* knot;

    str_init(&data->name);
    if (str_set_cstr(&data->name, name) != 0)
        goto set_name_failed;

    qwpos_vec_rb_init(&data->trails);
    bezier_knot_rb_init(&data->knots);
    bezier_segment_rb_init(&data->segments);
    qwaabb_rb_init(&data->segment_bbs);
    snake_split_rb_init(&data->splits);

    /* Need to create the initial knot before we can add a new segment */
    knot = bezier_knot_rb_emplace_realloc(&data->knots);
    if (knot == NULL)
        goto emplace_knot_failed;
    bezier_knot_init(knot, spawn_pos, QA_PI, 0, 0);

    if (add_new_segment(data, spawn_pos, make_qa(QA_PI)) != 0)
        goto add_segment_failed;

    qwpos_vec_init(&data->samples);

    return 0;

add_segment_failed:
emplace_knot_failed:
    qwpos_vec_rb_deinit(data->trails);
    snake_split_rb_deinit(data->splits);
    qwaabb_rb_deinit(data->segment_bbs);
    bezier_segment_rb_deinit(data->segments);
    bezier_knot_rb_deinit(data->knots);
    str_deinit(data->name);
set_name_failed:
    return -1;
}

/* ------------------------------------------------------------------------- */
static void snake_data_deinit(struct snake_data* data)
{
    qwpos_vec_deinit(data->samples);
    qwaabb_rb_deinit(data->segment_bbs);
    bezier_segment_rb_deinit(data->segments);
    bezier_knot_rb_deinit(data->knots);
    while (rb_count(data->trails) > 0)
        qwpos_vec_deinit(qwpos_vec_rb_take(data->trails));
    qwpos_vec_rb_deinit(data->trails);
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

    snake->hold = 0;
    snake->dead = 0;

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
            qa_mul(snake_turn_speed(param), make_qa2(60, sim_tick_rate)));
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
static void update_snake_aabb(struct snake_data* data)
{
    int i;
    data->bb = *rb_peek(data->segment_bbs, 0);
    for (i = 1; i < rb_count(data->segment_bbs); ++i)
    {
        struct qwaabb bb = *rb_peek(data->segment_bbs, i);
        data->bb = qwaabb_union(data->bb, bb);
    }
}

/* ------------------------------------------------------------------------- */
static void update_head_trail_aabb(struct snake_data* data)
{
    int                     i;
    struct qwaabb*          bb = rb_peek_write(data->segment_bbs);
    const struct qwpos_vec* trail = *rb_peek_write(data->trails);
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
static void update_head_segment(struct snake_data* data)
{
    bezier_calc_segment(
        rb_peek(data->segments, rb_count(data->segments) - 1),
        rb_peek(data->knots, rb_count(data->knots) - 1),
        rb_peek(data->knots, rb_count(data->knots) - 2));
}

/* ------------------------------------------------------------------------- */
static int
update_curve_from_head(struct snake_data* data, const struct snake_head* head)
{
    struct qwpos_vec** trail;
    double             error_squared;

    /* Append new position to the trail */
    trail = rb_peek_write(data->trails);
    qwpos_vec_push(trail, head->pos);

    /* Fit current bezier segment to trail */
    error_squared = bezier_fit_trail(
        rb_peek(data->knots, rb_count(data->knots) - 1),
        rb_peek(data->knots, rb_count(data->knots) - 2),
        *trail);

    /*
     * If the fit's error exceeds some threshold (determined empirically),
     * signal that a new segment needs to be created.
     */
    return error_squared > make_q16_16_2(1, 16);
}

/* ------------------------------------------------------------------------- */
static int measure_unused_segments(
    struct snake_data* data, const struct snake_param* param)
{
    struct bezier_sample sample;

    /* By sampling the curve from head to tail for the total length of the
     * snake, we can determine the number of segments that can be removed. */
    qwpos_vec_clear(data->samples);
    for (bezier_sample_begin(
             &sample, data->segments, SNAKE_PART_SPACING, snake_length(param));
         !bezier_sample_end(&sample);
         bezier_sample_next(&sample))
    {
        struct qwpos* pos = qwpos_vec_emplace(&data->samples);
        *pos = sample.pos;
    }
    return bezier_sample_idx(&sample);
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
    need_new_segment = update_curve_from_head(data, head);

    /*
     * Have to call these after updating curve data, because only then is the
     * point trail updated (and this is required for AABBs)
     */
    update_head_trail_aabb(data);
    update_head_segment(data);

    if (need_new_segment)
        if (add_new_segment(data, head->pos, head->angle) != 0)
            return -1;

    update_snake_aabb(data);
    return measure_unused_segments(data, param);
}

/* ------------------------------------------------------------------------- */
void snake_remove_stale_segments(struct snake_data* data, int stale_segments)
{
    CLITHER_DEBUG_ASSERT(stale_segments < rb_count(data->trails));

    while (stale_segments--)
        snake_remove_segment_from_tail(data);

    update_snake_aabb(data);
}

/* ------------------------------------------------------------------------- */
void snake_remove_stale_segments_with_rollback_constraint(
    struct snake_data* data, const struct snake_ack* ack, int stale_segments)
{
    CLITHER_DEBUG_ASSERT(stale_segments < rb_count(data->trails));

    while (stale_segments--)
    {
        /*
         * If at any point the acknowledged head position is on a curve
         * segment that we want to remove, abort, because this curve segment
         * is still required for rollback.
         */
        if (qwaabb_test_qwpos(*rb_peek_read(data->segment_bbs), ack->head.pos))
            break;

        snake_remove_segment_from_tail(data);
    }

    update_snake_aabb(data);
}

/* ------------------------------------------------------------------------- */
void snake_ack_frame(
    struct snake_data*        data,
    struct snake_ack*         ack,
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
            "snake_ack_frame(): Command buffer of snake \"%s\" is empty. "
            "Can't "
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
            "snake_ack_frame(): Frame number is outside of the command "
            "buffer "
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
     * It's possible the authoritative head position we receive from the
     * server goes through some packet loss, so may have to catch up.
     */
    while (u16_le_wrap(last_ackd_frame, frame_number))
    {
        /* "last_ackd_frame" refers to the next frame to simulate on the
         * ack'd head */
        struct cmd command = cmd_queue_take_or_predict(cmdq, last_ackd_frame);
        snake_step_head(&ack->head, param, command, sim_tick_rate);
        last_ackd_frame++;
    }

    /*
     * Our simulation of the last acknowledged head position diverges from
     * the server's head position. This means the predicted head position is
     * also incorrect.
     */
    if (snake_heads_are_equal(&ack->head, authoritative_head) == 0)
    {
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
            ack->head.pos.x,
            ack->head.pos.y,
            ack->head.angle,
            ack->head.speed,
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
        CLITHER_DEBUG_ASSERT(rb_count(data->trails) > 0);
        trail = *rb_peek_write(data->trails);
        while (u16_gt_wrap(predicted_frame, frame_number))
        {
            qwpos_vec_pop(trail);
            if (vec_count(trail) == 0)
            {
                remove_segment_from_head(data);

                /* Remove duplicate point */
                trail = *rb_peek_write(data->trails);
                qwpos_vec_pop(trail);
            }

            predicted_frame--;
        }

        /*
         * Restore head positions to authoritative state, which counts as
         * the first "step" forwards
         */
        ack->head = *authoritative_head;
        *predicted_head = *authoritative_head;
        if (update_curve_from_head(data, predicted_head))
        {
            update_head_trail_aabb(data);
            update_head_segment(data);
            add_new_segment(data, predicted_head->pos, predicted_head->angle);
        }

        /* Simulate head forwards again */
        cmd_queue_for_each (cmdq, i, frame, command)
        {
            snake_step_head(predicted_head, param, *command, sim_tick_rate);
            if (update_curve_from_head(data, predicted_head))
            {
                update_head_trail_aabb(data);
                update_head_segment(data);
                add_new_segment(
                    data, predicted_head->pos, predicted_head->angle);
            }
        }

        update_head_trail_aabb(data);
        update_head_segment(data);
        update_snake_aabb(data);
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

/* ------------------------------------------------------------------------- */
static int grow_bezier_knot_rb_for_knot_idx(
    struct bezier_knot_rb** knots, int16_t knot_idx)
{
    int16_t size;
    if (rb_capacity(*knots) > knot_idx)
        return 0;

    size = 2;
    while (size < knot_idx + 1)
        size *= 2;

    return bezier_knot_rb_resize(knots, size);
}

/* ------------------------------------------------------------------------- */
int snake_create_or_update_knot(
    struct snake_data* data,
    int16_t            knot_idx,
    struct qwpos       pos,
    qa                 angle,
    uint8_t            len_backwards,
    uint8_t            len_forwards)
{
    if (grow_bezier_knot_rb_for_knot_idx(&data->knots, knot_idx) != 0)
        return -1;

    bezier_knot_init(
        &data->knots->data[knot_idx], pos, angle, len_backwards, len_forwards);

    return 0;
}

/* ------------------------------------------------------------------------- */
int snake_update_bezier_extents(
    struct snake_data* data,
    int16_t            rb_read,
    int16_t            rb_write,
    uint8_t            head_len_backwards,
    uint8_t            second_len_forwards)
{
    int16_t i;
    char    do_full_recalculation;
    CLITHER_DEBUG_ASSERT(rb_read >= 0);
    CLITHER_DEBUG_ASSERT(rb_write >= 0);
    if (rb_read >= rb_capacity(data->knots) ||
        rb_write >= rb_capacity(data->knots))
    {
        return -1;
    }

    do_full_recalculation = 0;
    if (data->knots->read != rb_read || data->knots->write != rb_write)
        do_full_recalculation = 1;

    data->knots->read = rb_read;
    data->knots->write = rb_write;

    /* May need to recalculate all segments and bounding boxes */
    if (do_full_recalculation)
    {
        bezier_segment_rb_clear(data->segments);
        qwaabb_rb_clear(data->segment_bbs);

        for (i = 0; i < rb_count(data->knots) - 2; ++i)
        {
            struct bezier_segment*    segment;
            struct qwaabb*            bb;
            const struct bezier_knot* head = rb_peek(data->knots, i + 1);
            const struct bezier_knot* tail = rb_peek(data->knots, i + 0);
            segment = bezier_segment_rb_emplace_realloc(&data->segments);
            if (segment == NULL)
                return -1;
            bezier_calc_segment(segment, head, tail);

            bb = qwaabb_rb_emplace_realloc(&data->segment_bbs);
            if (bb == NULL)
                return -1;
            bezier_calc_aabb(bb, segment);
        }
    }

    /* The "len_forwards" property of the second knot (the one that
     * follows the head) and the "len_backwards" property of the head knot
     * are constantly changing. */
    if (rb_count(data->knots) > 1)
    {
        struct bezier_knot* head_knot = rb_peek_write(data->knots);
        struct bezier_knot* second_knot =
            rb_peek(data->knots, rb_count(data->knots) - 2);
        struct bezier_segment* segment = rb_peek_write(data->segments);
        struct qwaabb*         bb = rb_peek_write(data->segment_bbs);

        head_knot->len_backwards = head_len_backwards;
        second_knot->len_forwards = second_len_forwards;
        bezier_calc_segment(segment, head_knot, second_knot);
        bezier_calc_aabb(bb, segment);
    }

    update_snake_aabb(data);

    return 0;
}

/* ------------------------------------------------------------------------- */
struct qwpos snake_calculate_visible_range(const struct snake* snake)
{
    /* On a perfectly square screen, the width and height would be [2,2] */
    qw cam_scale = qw_mul(make_qw(2), snake_scale(&snake->param));
    /* Add some buffer for network latency */
    cam_scale = qw_add(cam_scale, make_qw(1));

    /* Most people are going to be playing on 16:9 */
    return make_qwposqw(qw_mul(cam_scale, make_qw2(16, 9)), cam_scale);
}

/* ------------------------------------------------------------------------- */
void snake_unextrapolate(
    struct snake_data*          data,
    struct snake_head*          head,
    const struct snake_replica* replica)
{
    *head = replica->head_history[0];

    if (rb_count(data->knots) > 1)
    {
        struct bezier_knot* head_knot = rb_peek_write(data->knots);
        head_knot->pos = head->pos;
        head_knot->angle = qa_add(head->angle, QA_PI);
    }
}

/* ------------------------------------------------------------------------- */
static int calc_T_inv_4x4(float T[4][4], float t1, float t2, float t3)
{
    int   i, j;
    float t1p2 = t1 * t1;
    float t1p3 = t1 * t1p2;
    float t2p2 = t2 * t2;
    float t2p3 = t2 * t2p2;
    float t3p2 = t3 * t3;
    float t3p3 = t3 * t3p2;

    /* clang-format off */
    float T_det = -t1p3*t2p2*t3 + t1p3*t2*t3p2 + t1p2*t2p3*t3 - t1p2*t2*t3p3 - t1*t2p3*t3p2 + t1*t2p2*t3p3;
    if (T_det == 0.0)
        return -1;

    T[0][0] = -t1p3*t2p2*t3 + t1p3*t2*t3p2 + t1p2*t2p3*t3 - t1p2*t2*t3p3 - t1*t2p3*t3p2 + t1*t2p2*t3p3;
    T[0][1] = 0;
    T[0][2] = 0;
    T[0][3] = 0;

    T[1][0] = t1p3*t2p2 - t1p3*t3p2 - t1p2*t2p3 + t1p2*t3p3 + t2p3*t3p2 - t2p2*t3p3;
    T[1][1] = -t2p3*t3p2 + t2p2*t3p3;
    T[1][2] = t1p3*t3p2 - t1p2*t3p3;
    T[1][3] = -t1p3*t2p2 + t1p2*t2p3;

    T[2][0] = -t1p3*t2 + t1p3*t3 + t1*t2p3 - t1*t3p3 - t2p3*t3 + t2*t3p3;
    T[2][1] = t2p3*t3 - t2*t3p3;
    T[2][2] = -t1p3*t3 + t1*t3p3;
    T[2][3] = t1p3*t2 - t1*t2p3;

    T[3][0] = t1p2*t2 - t1p2*t3 - t1*t2p2 + t1*t3p2 + t2p2*t3 - t2*t3p2;
    T[3][1] = -t2p2*t3 + t2*t3p2;
    T[3][2] = t1p2*t3 - t1*t3p2;
    T[3][3] = -t1p2*t2 + t1*t2p2;
    /* clang-format on */

    T_det = 1.0 / T_det;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            T[i][j] = T[i][j] * T_det;

    return 0;
}
static int calc_T_inv_3x3(float T[3][3], float t1, float t2)
{
    int   i, j;
    float t1p2 = t1 * t1;
    float t2p2 = t2 * t2;

    float T_det = t1 * t2p2 - t1p2 * t2;
    if (T_det == 0.0)
        return -1;

    T[0][0] = -t1 * t1 * t2 + t1 * t2 * t2;
    T[0][1] = 0;
    T[0][2] = 0;

    T[1][0] = t1p2 - t2p2;
    T[1][1] = t2p2;
    T[1][2] = -t1p2;

    T[2][0] = t2 - t1;
    T[2][1] = -t2;
    T[2][2] = t1;

    T_det = 1.0 / T_det;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            T[i][j] = T[i][j] * T_det;

    return 0;
}
static int calc_T_inv_2x2(float T[2][2], float t1)
{
    int   i, j;
    float T_det = t1;
    if (T_det == 0.0)
        return -1;

    T[0][0] = t1;
    T[0][1] = 0;

    T[1][0] = -1;
    T[1][1] = 1;

    T_det = 1.0 / T_det;
    for (i = 0; i < 2; i++)
        for (j = 0; j < 2; j++)
            T[i][j] = T[i][j] * T_det;

    return 0;
}

/* ------------------------------------------------------------------------- */
int snake_extrapolate(
    struct snake_data*          data,
    struct snake_head*          head,
    const struct snake_replica* replica,
    const struct snake_param*   param,
    uint16_t                    frame_number)
{
    qw                     dx, dy;
    struct bezier_knot*    head_knot;
    struct bezier_knot*    prev_knot;
    struct bezier_segment* segment;
    struct qwaabb*         segment_bb;

    if (rb_count(data->knots) < 2)
        return 0;

    dx = qw_sub(snake_boost_speed(param), snake_min_speed(param));
    dx = qw_rescale(dx, head->speed, 255);
    dx = qw_add(dx, snake_min_speed(param));
    dx = qw_mul(dx, make_qw(frame_number - replica->head_frame_numbers[0]));
    /* TODO: Clamp extrapolation distance here? */
    dx = qw_mul(qa_cos(head->angle), dx);
    head->pos.x = qw_add(head->pos.x, dx);

    dy = qw_sub(snake_boost_speed(param), snake_min_speed(param));
    dy = qw_rescale(dy, head->speed, 255);
    dy = qw_add(dy, snake_min_speed(param));
    dy = qw_mul(dy, make_qw(frame_number - replica->head_frame_numbers[0]));
    dy = qw_mul(qa_sin(head->angle), dy);
    head->pos.y = qw_add(head->pos.y, dy);

    head_knot = rb_peek_write(data->knots);
    prev_knot = rb_peek(data->knots, rb_count(data->knots) - 2);
    segment = rb_peek_write(data->segments);
    segment_bb = rb_peek_write(data->segment_bbs);

    head_knot->pos = head->pos;
    head_knot->angle = qa_add(head->angle, QA_PI);
    bezier_calc_segment(segment, head_knot, prev_knot);
    bezier_calc_aabb(segment_bb, segment);
    update_snake_aabb(data);

    return frame_number - replica->head_frame_numbers[0];
}

/* ------------------------------------------------------------------------- */
int snake_extrapolate_o2(
    struct snake_data*          data,
    struct snake_head*          head,
    const struct snake_replica* replica,
    uint16_t                    frame_number)
{
    float                  T[2][2];
    float                  a[2];
    float                  x[2];
    float                  t;
    float                  t0, t1;
    struct bezier_knot*    head_knot;
    struct bezier_knot*    prev_knot;
    struct bezier_segment* segment;
    struct qwaabb*         segment_bb;

    if (rb_count(data->knots) < 2)
        return 0;

    t0 = replica->head_frame_numbers[1];
    t1 = (float)(uint16_t)(replica->head_frame_numbers[0] - t0);
    if (calc_T_inv_2x2(T, t1) != 0)
        return 0;

    t = (float)(uint16_t)(frame_number - t0);
    x[0] = qw_to_float(replica->head_history[1].pos.x);
    x[1] = qw_to_float(replica->head_history[0].pos.x);
    a[0] = T[0][0] * x[0] + T[0][1] * x[1];
    a[1] = T[1][0] * x[0] + T[1][1] * x[1];
    head->pos.x = make_qw(a[0] + a[1] * t);

    x[0] = qw_to_float(replica->head_history[1].pos.y);
    x[1] = qw_to_float(replica->head_history[0].pos.y);
    a[0] = T[0][0] * x[0] + T[0][1] * x[1];
    a[1] = T[1][0] * x[0] + T[1][1] * x[1];
    head->pos.y = make_qw(a[0] + a[1] * t);

    head_knot = rb_peek_write(data->knots);
    prev_knot = rb_peek(data->knots, rb_count(data->knots) - 2);
    segment = rb_peek_write(data->segments);
    segment_bb = rb_peek_write(data->segment_bbs);

    head_knot->pos = head->pos;
    head_knot->angle = qa_add(head->angle, QA_PI);
    bezier_calc_segment(segment, head_knot, prev_knot);
    bezier_calc_aabb(segment_bb, segment);
    update_snake_aabb(data);

    return frame_number - replica->head_frame_numbers[0];
}

/* ------------------------------------------------------------------------- */
int snake_extrapolate_o3(
    struct snake_data*          data,
    struct snake_head*          head,
    const struct snake_replica* replica,
    uint16_t                    frame_number)
{
    float                  T[3][3];
    float                  a[3];
    float                  x[3];
    float                  t;
    float                  t0, t1, t2;
    struct bezier_knot*    head_knot;
    struct bezier_knot*    prev_knot;
    struct bezier_segment* segment;
    struct qwaabb*         segment_bb;

    if (rb_count(data->knots) < 2)
        return 0;

    t0 = replica->head_frame_numbers[2];
    t1 = (float)(uint16_t)(replica->head_frame_numbers[1] - t0);
    t2 = (float)(uint16_t)(replica->head_frame_numbers[0] - t0);
    if (calc_T_inv_3x3(T, t1, t2) != 0)
        return 0;

    t = (float)(uint16_t)(frame_number - t0);
    x[0] = qw_to_float(replica->head_history[2].pos.x);
    x[1] = qw_to_float(replica->head_history[1].pos.x);
    x[2] = qw_to_float(replica->head_history[0].pos.x);
    a[0] = T[0][0] * x[0] + T[0][1] * x[1] + T[0][2] * x[2];
    a[1] = T[1][0] * x[0] + T[1][1] * x[1] + T[1][2] * x[2];
    a[2] = T[2][0] * x[0] + T[2][1] * x[1] + T[2][2] * x[2];
    head->pos.x = make_qw(a[0] + a[1] * t + a[2] * t * t);

    x[0] = qw_to_float(replica->head_history[2].pos.y);
    x[1] = qw_to_float(replica->head_history[1].pos.y);
    x[2] = qw_to_float(replica->head_history[0].pos.y);
    a[0] = T[0][0] * x[0] + T[0][1] * x[1] + T[0][2] * x[2];
    a[1] = T[1][0] * x[0] + T[1][1] * x[1] + T[1][2] * x[2];
    a[2] = T[2][0] * x[0] + T[2][1] * x[1] + T[2][2] * x[2];
    head->pos.y = make_qw(a[0] + a[1] * t + a[2] * t * t);

    head_knot = rb_peek_write(data->knots);
    prev_knot = rb_peek(data->knots, rb_count(data->knots) - 2);
    segment = rb_peek_write(data->segments);
    segment_bb = rb_peek_write(data->segment_bbs);

    head_knot->pos = head->pos;
    head_knot->angle = qa_add(head->angle, QA_PI);
    bezier_calc_segment(segment, head_knot, prev_knot);
    bezier_calc_aabb(segment_bb, segment);
    update_snake_aabb(data);

    return frame_number - replica->head_frame_numbers[0];
}

/* ------------------------------------------------------------------------- */
int snake_extrapolate_o4(
    struct snake_data*          data,
    struct snake_head*          head,
    const struct snake_replica* replica,
    uint16_t                    frame_number)
{
    float                  T[4][4];
    float                  a[4];
    float                  x[4];
    float                  t;
    float                  t0, t1, t2, t3;
    struct bezier_knot*    head_knot;
    struct bezier_knot*    prev_knot;
    struct bezier_segment* segment;
    struct qwaabb*         segment_bb;

    if (rb_count(data->knots) < 2)
        return 0;

    t0 = replica->head_frame_numbers[3];
    t1 = (float)(uint16_t)(replica->head_frame_numbers[2] - t0);
    t2 = (float)(uint16_t)(replica->head_frame_numbers[1] - t0);
    t3 = (float)(uint16_t)(replica->head_frame_numbers[0] - t0);
    if (calc_T_inv_4x4(T, t1, t2, t3) != 0)
        return 0;

    t = (float)(uint16_t)(frame_number - t0);
    x[0] = qw_to_float(replica->head_history[3].pos.x);
    x[1] = qw_to_float(replica->head_history[2].pos.x);
    x[2] = qw_to_float(replica->head_history[1].pos.x);
    x[3] = qw_to_float(replica->head_history[0].pos.x);
    a[0] = T[0][0] * x[0] + T[0][1] * x[1] + T[0][2] * x[2] + T[0][3] * x[3];
    a[1] = T[1][0] * x[0] + T[1][1] * x[1] + T[1][2] * x[2] + T[1][3] * x[3];
    a[2] = T[2][0] * x[0] + T[2][1] * x[1] + T[2][2] * x[2] + T[2][3] * x[3];
    a[3] = T[3][0] * x[0] + T[3][1] * x[1] + T[3][2] * x[2] + T[3][3] * x[3];
    head->pos.x = make_qw(a[0] + a[1] * t + a[2] * t * t + a[3] * t * t * t);

    x[0] = qw_to_float(replica->head_history[3].pos.y);
    x[1] = qw_to_float(replica->head_history[2].pos.y);
    x[2] = qw_to_float(replica->head_history[1].pos.y);
    x[3] = qw_to_float(replica->head_history[0].pos.y);
    a[0] = T[0][0] * x[0] + T[0][1] * x[1] + T[0][2] * x[2] + T[0][3] * x[3];
    a[1] = T[1][0] * x[0] + T[1][1] * x[1] + T[1][2] * x[2] + T[1][3] * x[3];
    a[2] = T[2][0] * x[0] + T[2][1] * x[1] + T[2][2] * x[2] + T[2][3] * x[3];
    a[3] = T[3][0] * x[0] + T[3][1] * x[1] + T[3][2] * x[2] + T[3][3] * x[3];
    head->pos.y = make_qw(a[0] + a[1] * t + a[2] * t * t + a[3] * t * t * t);

    head_knot = rb_peek_write(data->knots);
    prev_knot = rb_peek(data->knots, rb_count(data->knots) - 2);
    segment = rb_peek_write(data->segments);
    segment_bb = rb_peek_write(data->segment_bbs);

    head_knot->pos = head->pos;
    head_knot->angle = qa_add(head->angle, QA_PI);
    bezier_calc_segment(segment, head_knot, prev_knot);
    bezier_calc_aabb(segment_bb, segment);
    update_snake_aabb(data);

    return frame_number - replica->head_frame_numbers[0];
}

/* ------------------------------------------------------------------------- */
static int remove_food_in_radius(morton morton, struct food* food, void* user)
{
    struct snake_param* param = user;
    (void)morton;
    snake_param_update(param, param->upgrades, param->food_eaten + food->value);
    return BMAP_ERASE;
}
int snake_eat_food(
    struct snake_head*  head,
    struct snake_param* param,
    struct food_bmap*   food_bmap)
{
    /* The "mouth" is a circle leading the head position. The distance and
     * radius depends on the snake's size as well as its upgrades */
    qw           mouth_radius = qw_mul(make_qw(0.15), snake_scale(param));
    struct qwpos mouth_pos = make_qwposqw(
        qw_add(head->pos.x, qw_mul(qa_cos(head->angle), mouth_radius)),
        qw_add(head->pos.y, qw_mul(qa_sin(head->angle), mouth_radius)));
    return food_bmap_for_each_in_radius(
        food_bmap, mouth_pos, mouth_radius, remove_food_in_radius, param);
}
