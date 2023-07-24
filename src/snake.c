#include "clither/bezier.h"
#include "clither/log.h"
#include "clither/q.h"
#include "clither/snake.h"
#include "clither/wrap.h"

#include "cstructures/memory.h"

#include <assert.h>

#define _USE_MATH_DEFINES
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define SNAKE_PART_SPACING make_qw2(1, 6)

/* ------------------------------------------------------------------------- */
void
snake_head_init(struct snake_head* head, struct qwpos spawn_pos)
{
    head->pos = spawn_pos;
    head->angle = make_qa(0);
    head->speed = 0;
}

/* ------------------------------------------------------------------------- */
static void
snake_data_init(struct snake_data* data, struct qwpos spawn_pos, const char* name)
{
    struct cs_vector* trail;

    data->name = MALLOC(strlen(name) + 1);
    strcpy(data->name, name);

    rb_init(&data->head_trails, sizeof(struct cs_vector));
    rb_init(&data->bezier_handles, sizeof(struct bezier_handle));
    rb_init(&data->bezier_aabbs, sizeof(struct qwaabb));
    vector_init(&data->bezier_points, sizeof(struct bezier_point));

    /*
     * Create the initial trail, which is the list of points the curve
     * is fitted to. This grows as the head moves forwards. Add the spawn pos
     * now, as we want the curve to begin there.
     */
    trail = rb_emplace(&data->head_trails);
    vector_init(trail, sizeof(struct qwpos));
    vector_push(trail, &spawn_pos);

    /*
     * Create the first bezier segment, which consists of two handles. By
     * convention all snakes start out facing to the right, but maybe this can
     * be changed in the future.
     */
    bezier_handle_init(rb_emplace(&data->bezier_handles), spawn_pos, make_qa(0));
    bezier_handle_init(rb_emplace(&data->bezier_handles), spawn_pos, make_qa(0));

    /* 
     * Create the curve's bounding box and also set the entire snake's bounding
     * box.
     */
    data->aabb = *(struct qwaabb*)rb_emplace(&data->bezier_aabbs) =
        make_qwaabbqw(spawn_pos.x, spawn_pos.y, spawn_pos.x, spawn_pos.y);
}

/* ------------------------------------------------------------------------- */
static void
snake_data_deinit(struct snake_data* data)
{
    vector_deinit(&data->bezier_points);
    rb_deinit(&data->bezier_aabbs);
    rb_deinit(&data->bezier_handles);

    RB_FOR_EACH(&data->head_trails, struct cs_vector, trail)
        vector_deinit(trail);
    VECTOR_END_EACH
    rb_deinit(&data->head_trails);

    FREE(data->name);
}

/* ------------------------------------------------------------------------- */
void
snake_init(struct snake* snake, struct qwpos spawn_pos, const char* name)
{
    command_rb_init(&snake->command_rb);
    snake_param_init(&snake->param);
    snake_data_init(&snake->data, spawn_pos, name);
    snake_head_init(&snake->head, spawn_pos);
    snake_head_init(&snake->head_ack, spawn_pos);

    snake->hold = 0;
}

/* ------------------------------------------------------------------------- */
void
snake_deinit(struct snake* snake)
{
    snake_data_deinit(&snake->data);
    command_rb_deinit(&snake->command_rb);
}

/* ------------------------------------------------------------------------- */
void
snake_step_head(
    struct snake_head* head,
    const struct snake_param* param,
    struct command command,
    uint8_t sim_tick_rate)
{
    qw dx, dy;
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
        head->angle = qa_sub(head->angle, qa_mul(snake_turn_speed(param), make_qa2(sim_tick_rate, 60)));
    else if (angle_diff < -snake_turn_speed(param))
        head->angle = qa_add(head->angle, snake_turn_speed(param));
    else
        head->angle = target_angle;

    /* Integrate speed over time */
    target_speed = command.action == COMMAND_ACTION_BOOST ?
        255 :
        qw_sub(snake_max_speed(param), snake_min_speed(param)) * command.speed / qw_sub(snake_boost_speed(param), snake_min_speed(param));
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
static void
snake_update_aabb(struct snake_data* data)
{
    int i;

    data->aabb = *(struct qwaabb*)rb_peek(&data->bezier_aabbs, 0);
    for (i = 1; i < (int)rb_count(&data->bezier_aabbs); ++i)
    {
        data->aabb = qwaabb_union(
            data->aabb,
            *(struct qwaabb*)rb_peek(&data->bezier_aabbs, i));
    }
}

/* ------------------------------------------------------------------------- */
/*!
 * \brief Updates the front-most segment of the curve (the head).
 */
static void
snake_update_head_trail_aabb(struct snake_data* data)
{
    int i;
    struct qwaabb* bb = rb_peek_write(&data->bezier_aabbs);
    const struct cs_vector* trail = rb_peek_write(&data->head_trails);
    const struct qwpos* p = vector_get(trail, 0);

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
    for (i = 1; i < (int)vector_count(trail); ++i)
    {
        p = vector_get(trail, i);
        if (bb->x1 > p->x) bb->x1 = p->x;
        if (bb->x2 < p->x) bb->x2 = p->x;
        if (bb->y1 > p->y) bb->y1 = p->y;
        if (bb->y2 < p->y) bb->y2 = p->y;
    }
}

/* ------------------------------------------------------------------------- */
static int
snake_update_curve_from_head(struct snake_data* data, const struct snake_head* head)
{
    struct cs_vector* trail;
    double error_squared;

    /* Append new position to the trail */
    trail = rb_peek_write(&data->head_trails);
    vector_push(trail, &head->pos);

    /* Fit current bezier segment to trail */
    error_squared = bezier_fit_trail(
        rb_peek(&data->bezier_handles, rb_count(&data->bezier_handles) - 1),
        rb_peek(&data->bezier_handles, rb_count(&data->bezier_handles) - 2),
        trail);

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
    struct cs_vector* trail = rb_emplace(&data->head_trails);
    vector_init(trail, sizeof(struct qwpos));
    vector_push(trail, &head->pos);

    /*
     * Add a new bezier handle. Since there is only one datapoint, the curve
     * is completely defined by the current head position.
     */
    bezier_handle_init(rb_emplace(&data->bezier_handles), head->pos, qa_add(head->angle, QA_PI));

    /* Add a new bounding box, which is also defined by the current head position */
    *(struct qwaabb*)rb_emplace(&data->bezier_aabbs) =
        make_qwaabbqw(head->pos.x, head->pos.y, head->pos.x, head->pos.y);
}

/* ------------------------------------------------------------------------- */
int
snake_step(
    struct snake_data* data,
    struct snake_head* head,
    const struct snake_param* param,
    struct command command,
    uint8_t sim_tick_rate)
{
    int need_new_segment;

    snake_step_head(head, param, command, sim_tick_rate);

    /* 
     * Since AABBs are derived from the point trails and not from the curve,
     * it's ok to call these before updating the curve.
     */
    snake_update_head_trail_aabb(data);
    snake_update_aabb(data);

    if (snake_update_curve_from_head(data, head))
        snake_add_new_segment(data, head);

    bezier_squeeze_step(&data->bezier_handles, sim_tick_rate);

    /* This function returns the number of segments that are superfluous. */
    return bezier_calc_equidistant_points(
        &data->bezier_points,
        &data->bezier_handles,
        qw_mul(SNAKE_PART_SPACING, snake_scale(param)),
        snake_length(param));
}

/* ------------------------------------------------------------------------- */
void
snake_remove_stale_segments(struct snake_data* data, int stale_segments)
{
    assert(stale_segments < (int)rb_count(&data->head_trails));

    while (stale_segments--)
    {
        vector_deinit(rb_take(&data->head_trails));
        rb_take(&data->bezier_handles);
        rb_take(&data->bezier_aabbs);
    }

    snake_update_aabb(data);
}

/* ------------------------------------------------------------------------- */
void
snake_remove_stale_segments_with_rollback_constraint(
    struct snake_data* data,
    const struct snake_head* head_ack,
    int stale_segments)
{
    assert(stale_segments < (int)rb_count(&data->head_trails));

    while (stale_segments--)
    {
        /* 
         * If at any point the acknowledged head position is on a curve segment
         * that we want to remove, abort, because this curve segment is still
         * required for rollback.
         */
        if (qwaabb_qwpos(*(struct qwaabb*)rb_peek_read(&data->bezier_aabbs), head_ack->pos))
            break;

        vector_deinit(rb_take(&data->head_trails));
        rb_take(&data->bezier_handles);
        rb_take(&data->bezier_aabbs);
    }

    snake_update_aabb(data);
}

/* ------------------------------------------------------------------------- */
void
snake_ack_frame(
    struct snake_data* data,
    struct snake_head* acknowledged_head,
    struct snake_head* predicted_head,
    const struct snake_head* authoritative_head,
    const struct snake_param* param,
    struct command_rb* command_rb,
    uint16_t frame_number,
    uint8_t sim_tick_rate)
{
    uint16_t last_ackd_frame, predicted_frame;

    if (command_rb_count(command_rb) == 0)
    {
        log_warn("snake_ack_frame(): Command buffer of snake \"%s\" is empty. Can't step.\n", data->name);
        return;
    }
    last_ackd_frame = command_rb_frame_begin(command_rb);
    predicted_frame = command_rb_frame_end(command_rb);

    /* last_ackd_frame <= frame_number <= predicted_frame */
    if (u16_lt_wrap(frame_number, last_ackd_frame) || u16_gt_wrap(frame_number, predicted_frame))
    {
        log_warn("snake_ack_frame(): Frame number is outside of the command buffer range! Something is very wrong.\n"
            "  frame_number=%d\n"
            "  last_ackd_frame=%d\n"
            "  predicted_frame=%d\n",
            frame_number, last_ackd_frame, predicted_frame);
        return;
    }

    /*
     * It's possible the authoritative head position we receive from the server
     * goes through some packet loss, so may have to catch up.
     */
    while (u16_le_wrap(last_ackd_frame, frame_number))
    {
        /* "last_ackd_frame" refers to the next frame to simulate on the ack'd head */
        struct command command = command_rb_take_or_predict(command_rb, last_ackd_frame);
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
        int handles_to_squeeze;

        log_dbg("Rollback from frame %d to %d\n"
            "  ackd head: pos=%d,%d, angle=%d, speed=%d\n"
            "  auth head: pos=%d,%d, angle=%d, speed=%d\n",
            predicted_frame, frame_number,
            acknowledged_head->pos.x, acknowledged_head->pos.y, acknowledged_head->angle, acknowledged_head->speed,
            authoritative_head->pos.x, authoritative_head->pos.y, authoritative_head->angle, authoritative_head->speed);

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
        assert(rb_count(&data->head_trails) > 0);
        struct cs_vector* trail = rb_peek_write(&data->head_trails);
        while (u16_gt_wrap(predicted_frame, frame_number))
        {
            vector_pop(trail);
            if (vector_count(trail) == 0)
            {
                vector_deinit(rb_takew(&data->head_trails));
                assert(rb_count(&data->head_trails) > 0);
                trail = rb_peek_write(&data->head_trails);

                vector_pop(trail);  /* Remove duplicate point */

                rb_takew(&data->bezier_handles);
                rb_takew(&data->bezier_aabbs);
            }

            predicted_frame--;
        }

        /* Restore head positions to authoritative state */
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
        COMMAND_RB_FOR_EACH(command_rb, frame, command)
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
            bezier_squeeze_n_recent_step(&data->bezier_handles, handles_to_squeeze, sim_tick_rate);
        COMMAND_RB_END_EACH

        snake_update_head_trail_aabb(data);
        snake_update_aabb(data);

        /* TODO: distance is a function of the snake's length */
        bezier_calc_equidistant_points(
            &data->bezier_points,
            &data->bezier_handles,
            qw_mul(SNAKE_PART_SPACING, snake_scale(param)),
            snake_length(param));
    }

    /* TODO: remove curve segments */
}

/* ------------------------------------------------------------------------- */
char
snake_is_held(struct snake* snake, uint16_t frame_number)
{
    if (command_rb_count(&snake->command_rb) > 0 &&
        command_rb_frame_begin(&snake->command_rb) == frame_number)
    {
        snake->hold = 0;
    }

    return snake->hold;
}
