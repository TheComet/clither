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
void
snake_init(struct snake* snake, struct qwpos spawn_pos, const char* name)
{
    snake->name = MALLOC(strlen(name) + 1);
    strcpy(snake->name, name);

    vector_init(&snake->points, sizeof(struct qwpos));
    vector_init(&snake->bezier_handles, sizeof(struct bezier_handle));
    vector_init(&snake->bezier_points, sizeof(struct bezier_point));

    bezier_handle_init(vector_emplace(&snake->bezier_handles), spawn_pos);
    bezier_handle_init(vector_emplace(&snake->bezier_handles), spawn_pos);

    snake->head.pos = spawn_pos;
    snake->head.angle = make_qa(0);
    snake->head.speed = 0;

    snake->length = 200;
}

/* ------------------------------------------------------------------------- */
void
snake_deinit(struct snake* snake)
{
    FREE(snake->name);

    vector_deinit(&snake->bezier_points);
    vector_deinit(&snake->bezier_handles);
    vector_deinit(&snake->points);
}

/* ------------------------------------------------------------------------- */
void
controls_buffer(struct cs_btree* buffer, const struct controls* controls, uint16_t frame_number)
{
    struct controls* existing;
    if (btree_insert_or_get(buffer, frame_number, controls, (void**)&existing) == BTREE_EXISTS)
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
controls_ack(struct cs_btree* buffer, uint16_t frame_number)
{
    /*
     * Erase all controls that predate the frame number being acknowledged.
     *
     * We keep the current frame's controls, because it may be required later
     * for prediction.
     */
    while (btree_count(buffer) > 0 && u16_lt_wrap(*BTREE_KEY(buffer, 0), frame_number))
        btree_erase_index(buffer, 0);
}

/* ------------------------------------------------------------------------- */
struct controls*
controls_get_or_predict(struct cs_btree* buffer, uint16_t frame_number)
{
    struct controls* prev_controls;
    struct controls* controls;
    uint16_t next_frame_number = frame_number + 1;

    controls = btree_find(buffer, next_frame_number);
    if (controls != NULL)
        return controls;

    if (btree_count(buffer) == 0)
    {
        log_warn("Controls buffer was empty! Should never happen\n");
        controls = btree_emplace_new(buffer, next_frame_number);
        controls_init(controls);  /* Default controls */
        return controls;
    }

    /* Prediction of next controls. For now we simply copy the previously known controls */
    controls = btree_emplace_new(buffer, next_frame_number);
    prev_controls = btree_find(buffer, frame_number);
    if (prev_controls != NULL)
        *controls = *prev_controls;
    else
    {
        log_warn("Previous frame's controls don't exist. Can't predict.\n");
        controls_init(controls);  /* Default controls */
    }

    return controls;
}

/* ------------------------------------------------------------------------- */
void
snake_ack_head(
        struct snake_head* pred,
        struct snake_head* ackd,
        const struct snake_head* auth,
        const struct cs_btree* buffer,
        uint16_t frame_number,
        uint8_t sim_tick_rate)
{
    uint16_t last_ackd_frame = btree_first_key(buffer);
    uint16_t predicted_frame = btree_last_key(buffer);

    /* last_ackd_frame <= frame_number <= predicted_frame */
    assert(u16_le_wrap(last_ackd_frame, frame_number));
    assert(u16_le_wrap(frame_number, predicted_frame));

    /*
     * It's possible the authoritative head position we receive from the server
     * goes through some packet loss, so may have to catch up.
     */
    if ((uint16_t)(frame_number - last_ackd_frame) > 0x7FFF)
        log_warn("Last acknowledged frame is way behind the current frame number! Acknowledged frame: %d, current frame: %d\n",
            last_ackd_frame, frame_number);
    else
        while (last_ackd_frame != frame_number)
        {
            last_ackd_frame++;
            snake_step_head(ackd, btree_find(buffer, last_ackd_frame), sim_tick_rate);
        }

    /*
     * Our simulation of the last acknowledged head position diverges from the
     * server's head position. This means the predicted head position is also
     * incorrect.
     */
    if (snake_heads_are_equal(ackd, auth) == 0)
    {
        /* Roll back head to authoritative state */
        *ackd = *auth;
        *pred = *auth;

        /* Simulate head forwards again */
        BTREE_FOR_EACH(buffer, struct controls, frame, controls)
            /*
             * Skip all controls in frames up to and including the currently
             * acknowledged frame number, because that's what we rolled back
             * to.
             */
            if (u16_le_wrap(frame, frame_number))
                continue;

            snake_step_head(pred, controls, sim_tick_rate);
        BTREE_END_EACH
    }
}

/* ------------------------------------------------------------------------- */
void
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
    if (angle_diff > make_qa(M_PI))
        angle_diff = qa_sub(angle_diff, make_qa(2 * M_PI));
    else if (angle_diff < make_qa(-M_PI))
        angle_diff = qa_add(angle_diff, make_qa(2 * M_PI));

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

    /* Ensure the angle remains in the range [-pi .. pi) or bad things happen */
    if (head->angle < make_qa(-M_PI))
        head->angle = qa_add(head->angle, make_qa(2 * M_PI));
    else if (head->angle >= make_qa(M_PI))
        head->angle = qa_sub(head->angle, make_qa(2 * M_PI));

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
void
snake_update_curve(struct snake* snake, uint16_t frame_number, uint8_t sim_tick_rate)
{
    double error_squared;

    /* Append new position to the list of points */
    vector_push(&snake->points, &snake->head.pos);

    /* Fit current bezier segment to list of points */
    error_squared = bezier_fit_head(
        vector_get_element(&snake->bezier_handles, vector_count(&snake->bezier_handles) - 1),
        vector_get_element(&snake->bezier_handles, vector_count(&snake->bezier_handles) - 2),
        &snake->points);

    /* Make tight circles become tighter over time */
    bezier_squeeze_step(&snake->bezier_handles, sim_tick_rate);

    /* Update equidistant points -- required by rendering code */
    /* TODO: Distance is a function of the snake's size/length */
    bezier_calc_equidistant_points(&snake->bezier_points, &snake->bezier_handles, make_qw2(1, 32), snake->length);

    /*
     * If the fit's error exceeds some threshold (determined empirically),
     * create a new bezier segment.
     */
    if (error_squared > make_q16_16_2(1, 16))
    {
        struct bezier_handle* head = vector_back(&snake->bezier_handles);
        struct qwpos head_pos = head->pos;
        bezier_handle_init(vector_emplace(&snake->bezier_handles), head_pos);
        vector_clear(&snake->points);
        vector_push(&snake->points, &snake->head.pos);
    }
}
