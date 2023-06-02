#include "clither/bezier.h"
#include "clither/controls.h"
#include "clither/log.h"
#include "clither/q.h"
#include "clither/snake.h"
#include "clither/wrap.h"

#include "cstructures/memory.h"

#include <stdio.h>

#define _USE_MATH_DEFINES
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define TURN_SPEED   make_qa2(1, 16)
#define MIN_SPEED    make_qw2(1, 256)
#define MAX_SPEED    make_qw2(1, 128)
#define BOOST_SPEED  make_qw2(1, 64)
#define ACCELERATION         (1.0 / 32)

/* ------------------------------------------------------------------------- */
static void
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
    struct cs_vector* points;

    data->name = MALLOC(strlen(name) + 1);
    strcpy(data->name, name);

    vector_init(&data->points, sizeof(struct cs_vector));
    vector_init(&data->bezier_handles, sizeof(struct bezier_handle));
    vector_init(&data->bezier_points, sizeof(struct bezier_point));

    points = vector_emplace(&data->points);
    vector_init(points, sizeof(struct qwpos));
    vector_push(points, &spawn_pos);
    bezier_handle_init(vector_emplace(&data->bezier_handles), spawn_pos);
    bezier_handle_init(vector_emplace(&data->bezier_handles), spawn_pos);

    vector_resize(&data->bezier_points, 200);
}

/* ------------------------------------------------------------------------- */
static void
snake_data_deinit(struct snake_data* data)
{
    vector_deinit(&data->bezier_points);
    vector_deinit(&data->bezier_handles);

    VECTOR_FOR_EACH(&data->points, struct cs_vector, points)
        vector_deinit(points);
    VECTOR_END_EACH
    vector_deinit(&data->points);

    FREE(data->name);
}

/* ------------------------------------------------------------------------- */
void
snake_init(struct snake* snake, struct qwpos spawn_pos, const char* name)
{
    btree_init(&snake->controls_buffer, sizeof(struct controls));
    snake_data_init(&snake->data, spawn_pos, name);
    snake_head_init(&snake->head, spawn_pos);
    snake_head_init(&snake->head_ack, spawn_pos);
}

/* ------------------------------------------------------------------------- */
void
snake_deinit(struct snake* snake)
{
    snake_data_deinit(&snake->data);
    btree_deinit(&snake->controls_buffer);
}

/* ------------------------------------------------------------------------- */
void
controls_add(struct cs_btree* controls_buffer, const struct controls* controls, uint16_t frame_number)
{
    struct controls* existing;
    if (btree_insert_or_get(controls_buffer, frame_number, controls, (void**)&existing) == BTREE_EXISTS)
        if (controls->angle != existing->angle ||
            controls->speed != existing->speed ||
            controls->action != existing->action)
        {
            log_warn("Tried buffering controls on frame %d, but new controls are different from existing!\n"
                "  current  : angle=%d, speed=%d, action=%d\n"
                "  received : angle=%d, speed=%d, action=%d\n",
                     frame_number,
                     existing->angle, existing->speed, existing->action,
                     controls->angle, controls->speed, controls->action);
        }
}

/* ------------------------------------------------------------------------- */
void
controls_ack(struct cs_btree* controls_buffer, uint16_t frame_number)
{
    /*
     * Erase all controls that predate the frame number being acknowledged.
     *
     * We keep the current frame's controls and also make sure there is always
     * at least one entry in the buffer. It may be required later for prediction. 
     */
    while (btree_count(controls_buffer) > 1 &&
        u16_lt_wrap(*BTREE_KEY(controls_buffer, 0), frame_number))
    {
        btree_erase_index(controls_buffer, 0);
    }
}

/* ------------------------------------------------------------------------- */
struct controls
controls_get_or_predict(const struct cs_btree* controls_buffer, uint16_t frame_number)
{
    struct controls* controls;
    uint16_t first_frame_number;

    controls = btree_find(controls_buffer, frame_number);
    if (controls != NULL)
        return *controls;
    log_dbg("controls_get_or_predict(): No controls for frame %d, predicting...\n", frame_number);

    frame_number--;  /* Try to find previous frame */
    first_frame_number = btree_first_key(controls_buffer);
    if (btree_count(controls_buffer) == 0 || u16_lt_wrap(frame_number, first_frame_number))
    {
        struct controls c;
        log_warn("Previous frame's controls don't exist. Can't predict. Using default controls as fallback\n");
        controls_init(&c);  /* Default controls */
        return c;
    }

    /* Prediction of next controls. For now we simply copy the previously known controls */
    while (u16_ge_wrap(frame_number, first_frame_number))
    {
        controls = btree_find(controls_buffer, frame_number);
        if (controls != NULL)
            break;
        frame_number--;
    }

    return *controls;
}

/* ------------------------------------------------------------------------- */
static void
snake_step_head(struct snake_head* head, const struct controls* controls, uint8_t sim_tick_rate)
{
    double a;
    qw dx, dy;
    qa angle_diff, target_angle;
    uint8_t target_speed;

    /*
     * The "controls" structure contains the absolute angle (in world space) of
     * the desired angle. That is, the angle from the snake's head to the mouse
     * cursor. It is stored in an unsigned char [0 .. 255]. We need to convert
     * it to radians [-pi .. pi) using the fixed point angle type "qa"
     */
    target_angle = make_qa(controls->angle / 256.0 * 2 * M_PI - M_PI);

    /* Calculate difference between desired angle and actual angle and wrap */
    angle_diff = qa_sub(head->angle, target_angle);

    /*
     * Turn the head towards the target angle and make sure to not exceed the
     * maximum turning speed.
     */
    if (angle_diff > TURN_SPEED)
        head->angle = qa_sub(head->angle, qa_mul(TURN_SPEED, make_qa2(sim_tick_rate, 60)));
    else if (angle_diff < -TURN_SPEED)
        head->angle = qa_add(head->angle, TURN_SPEED);
    else
        head->angle = target_angle;

    /* Integrate speed over time */
    target_speed = controls->action == CONTROLS_ACTION_BOOST ?
        255 :
        qw_sub(MAX_SPEED, MIN_SPEED) * controls->speed / qw_sub(BOOST_SPEED, MIN_SPEED);
    if (head->speed - target_speed > (ACCELERATION * 256))
        head->speed -= (ACCELERATION * 256);
    else if (head->speed - target_speed < (-ACCELERATION * 256))
        head->speed += (ACCELERATION * 256);
    else
        head->speed = target_speed;

    /* Update snake position using the head's current angle and speed */
    a = qa_to_float(head->angle);
    dx = make_qw(cos(a) * (head->speed / 255.0 * qw_to_float(BOOST_SPEED - MIN_SPEED) + qw_to_float(MIN_SPEED)));
    dy = make_qw(sin(a) * (head->speed / 255.0 * qw_to_float(BOOST_SPEED - MIN_SPEED) + qw_to_float(MIN_SPEED)));
    head->pos.x = qw_add(head->pos.x, dx);
    head->pos.y = qw_add(head->pos.y, dy);
}

/* ------------------------------------------------------------------------- */
static int
snake_update_curve_from_head(struct snake_data* data, const struct snake_head* head)
{
    double error_squared;
    struct cs_vector* points;

    /* Append new position to the list of points */
    points = vector_back(&data->points);
    vector_push(points, &head->pos);

    /* Fit current bezier segment to list of points */
    error_squared = bezier_fit_head(
        vector_get_element(&data->bezier_handles, vector_count(&data->bezier_handles) - 1),
        vector_get_element(&data->bezier_handles, vector_count(&data->bezier_handles) - 2),
        points);

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
    struct bezier_handle* head_handle = vector_back(&data->bezier_handles);
    struct qwpos head_handle_pos = head_handle->pos;
    struct cs_vector* points = vector_emplace(&data->points);
    bezier_handle_init(vector_emplace(&data->bezier_handles), head_handle_pos);
    vector_init(points, sizeof(struct qwpos));
    vector_push(points, &head->pos);
}

/* ------------------------------------------------------------------------- */
void
snake_step(
    struct snake_data* data,
    struct snake_head* head,
    struct controls* controls,
    uint8_t sim_tick_rate)
{
    snake_step_head(head, controls, sim_tick_rate);
    if (snake_update_curve_from_head(data, head))
        snake_add_new_segment(data, head);
    bezier_squeeze_step(&data->bezier_handles, sim_tick_rate);
    /* TODO: distance is a function of the snake's length */
    bezier_calc_equidistant_points(&data->bezier_points, &data->bezier_handles, make_qw2(1, 16), snake_length(data));
}

/* ------------------------------------------------------------------------- */
void
snake_ack_frame(
    struct snake_data* data,
    struct snake_head* acknowledged_head,
    struct snake_head* predicted_head,
    const struct snake_head* authoritative_head,
    struct cs_btree* controls_buffer,
    uint16_t frame_number,
    uint8_t sim_tick_rate)
{
    uint16_t last_ackd_frame, predicted_frame;

    if (btree_count(controls_buffer) == 0)
    {
        log_warn("snake_ack_frame(): Controls buffer of snake \"%s\" is empty\n", data->name);
        return;
    }
    last_ackd_frame = btree_first_key(controls_buffer);
    predicted_frame = btree_last_key(controls_buffer);

    /* last_ackd_frame <= frame_number <= predicted_frame */
    if (u16_lt_wrap(frame_number, last_ackd_frame) || u16_gt_wrap(frame_number, predicted_frame))
    {
        log_warn("snake_ack_frame(): Frame number is outside of the controls buffer range! Something is very wrong.\n"
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
    while (last_ackd_frame != frame_number)
    {
        struct controls* controls;

        /* "last_ackd_frame" refers to a frame that was already simulated */
        last_ackd_frame++;
        controls = btree_find(controls_buffer, last_ackd_frame);
        assert(controls != NULL);

        /* Step to next frame */
        snake_step_head(acknowledged_head, controls, sim_tick_rate);
    }
    controls_ack(controls_buffer, frame_number);

    /*
     * Our simulation of the last acknowledged head position diverges from the
     * server's head position. This means the predicted head position is also
     * incorrect.
     */
    if (snake_heads_are_equal(acknowledged_head, authoritative_head) == 0)
    {
        int handles_to_squeeze;

        log_dbg("Rollback to frame %d\n"
            "  ackd head: pos=%x,%x, angle=%x, speed=%x\n"
            "  auth head: pos=%x,%x, angle=%x, speed=%x\n",
            frame_number,
            acknowledged_head->pos.x, acknowledged_head->pos.y, acknowledged_head->angle, acknowledged_head->speed,
            authoritative_head->pos.x, authoritative_head->pos.y, authoritative_head->angle, authoritative_head->speed);

        /*
         * Remove all points generated since the last acknowledged frame.
         * In rare cases this may be on the boundary of two bezier segments,
         * in which case we must also remove the segment and corresponding
         * points list.
         * 
         * Also note that the first and last points in two points lists share
         * the same position, so when removing a bezier segment, two points
         * need to be removed.
         */
        struct cs_vector* points = vector_back(&data->points);
        while (u16_gt_wrap(predicted_frame, frame_number))
        {
            vector_pop(points);
            if (vector_count(points) == 0)
            {
                vector_deinit(vector_pop(&data->points));
                points = vector_back(&data->points);
                vector_pop(points);  /* Duplicate point */

                vector_pop(&data->bezier_handles);
            }

            predicted_frame--;
        }

        /* Restore head positions to authoritative state */
        *acknowledged_head = *authoritative_head;
        *predicted_head = *authoritative_head;

        /* Simulate head forwards again */
        handles_to_squeeze = 0;
        BTREE_FOR_EACH(controls_buffer, struct controls, frame, controls)
            /*
             * Skip all controls in frames up to and including the currently
             * acknowledged frame number, because that's what we rolled back
             * to.
             */
            if (u16_le_wrap(frame, frame_number))
                continue;

            snake_step_head(predicted_head, controls, sim_tick_rate);
            if (snake_update_curve_from_head(data, predicted_head))
            {
                snake_add_new_segment(data, predicted_head);
                handles_to_squeeze++;
            }

            log_dbg("Roll forwards, frame=%d\n"
                "  controls : %x,%x\n"
                "  pred head: pos=%x,%x, angle=%x, speed=%x\n",
                frame,
                controls->angle, controls->speed,
                predicted_head->pos.x, predicted_head->pos.y, predicted_head->angle, predicted_head->speed);

            /* 
             * The snake's bezier handles are "squeezed" over time. Only have
             * to re-squeeze the handles that were re-created during forwards
             * simulation.
             */
            bezier_squeeze_n_recent_step(&data->bezier_handles, handles_to_squeeze, sim_tick_rate);
        BTREE_END_EACH

        /* TODO: distance is a function of the snake's length */
        bezier_calc_equidistant_points(&data->bezier_points, &data->bezier_handles, make_qw2(1, 16), snake_length(data));
    }
}
