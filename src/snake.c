#include "clither/bezier.h"
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
#define MIN_SPEED    make_qw2(1, 96)
#define MAX_SPEED    make_qw2(1, 48)
#define BOOST_SPEED  make_qw2(1, 16)
#define ACCELERATION         (1.0 / 32)

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
    struct cs_vector* points;

    data->name = MALLOC(strlen(name) + 1);
    strcpy(data->name, name);

    vector_init(&data->points, sizeof(struct cs_vector));
    vector_init(&data->bezier_handles, sizeof(struct bezier_handle));
    vector_init(&data->bezier_points, sizeof(struct bezier_point));

    points = vector_emplace(&data->points);
    vector_init(points, sizeof(struct qwpos));
    vector_push(points, &spawn_pos);
    bezier_handle_init(vector_emplace(&data->bezier_handles), spawn_pos, make_qa(0));
    bezier_handle_init(vector_emplace(&data->bezier_handles), spawn_pos, make_qa(0));

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
    command_rb_init(&snake->command_rb);
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
snake_step_head(struct snake_head* head, struct command command, uint8_t sim_tick_rate)
{
    double a;
    qw dx, dy;
    uint8_t target_speed;

    /*
     * The command structure contains the absolute angle (in world space) of
     * the desired angle. That is, the angle from the snake's head to the mouse
     * cursor. It is stored in an unsigned char [0 .. 255]. We need to convert
     * it to radians [-pi .. pi) using the fixed point angle type "qa"
     */
    qa target_angle = command_angle_to_qa(command);

    /* Calculate difference between desired angle and actual angle and wrap */
    qa angle_diff = qa_sub(head->angle, target_angle);

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
    target_speed = command.action == COMMAND_ACTION_BOOST ?
        255 :
        qw_sub(MAX_SPEED, MIN_SPEED) * command.speed / qw_sub(BOOST_SPEED, MIN_SPEED);
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
    struct cs_vector* points = vector_emplace(&data->points);
    bezier_handle_init(vector_emplace(&data->bezier_handles), head->pos, qa_add(head->angle, make_qa(M_PI)));
    vector_init(points, sizeof(struct qwpos));
    vector_push(points, &head->pos);
}

/* ------------------------------------------------------------------------- */
void
snake_step(
    struct snake_data* data,
    struct snake_head* head,
    struct command command,
    uint8_t sim_tick_rate)
{
    snake_step_head(head, command, sim_tick_rate);
    if (snake_update_curve_from_head(data, head))
        snake_add_new_segment(data, head);
    bezier_squeeze_step(&data->bezier_handles, sim_tick_rate);
    /* TODO: distance is a function of the snake's length */
    bezier_calc_equidistant_points(&data->bezier_points, &data->bezier_handles, make_qw2(1, 6), 100);
}

/* ------------------------------------------------------------------------- */
void
snake_ack_frame(
    struct snake_data* data,
    struct snake_head* acknowledged_head,
    struct snake_head* predicted_head,
    const struct snake_head* authoritative_head,
    struct command_rb* command_rb,
    uint16_t frame_number,
    uint8_t sim_tick_rate)
{
    uint16_t last_ackd_frame, predicted_frame;

    if (command_rb_count(command_rb) == 0)
    {
        log_warn("snake_ack_frame(): Command buffer of snake \"%s\" is empty\n", data->name);
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

        /* Step to next frame */
        snake_step_head(acknowledged_head, command, sim_tick_rate);

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
        handles_to_squeeze = 0;
        if (snake_update_curve_from_head(data, predicted_head))
        {
            snake_add_new_segment(data, predicted_head);
            handles_to_squeeze++;
        }

        /* Simulate head forwards again */
        COMMAND_RB_FOR_EACH(command_rb, frame, command)
            snake_step_head(predicted_head, *command, sim_tick_rate);
            if (snake_update_curve_from_head(data, predicted_head))
            {
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

        /* TODO: distance is a function of the snake's length */
        bezier_calc_equidistant_points(&data->bezier_points, &data->bezier_handles, make_qw2(1, 16), snake_length(data));
    }
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
