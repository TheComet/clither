#include "clither/bezier.h"
#include "clither/q.h"
#include "clither/snake.h"
#include "clither/log.h"

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
#define ACCELERATION (1.0 / 32)

/* ------------------------------------------------------------------------- */
void
snake_init(struct snake* snake, const char* name)
{
    snake->name = MALLOC(strlen(name) + 1);
    strcpy(snake->name, name);

    controls_init(&snake->controls);

    snake->head_pos = make_qwpos(0, 0);
    snake->head_angle = make_qa(0);
    snake->speed = 0;

    vector_init(&snake->points, sizeof(struct qwpos));
    vector_init(&snake->bezier_handles, sizeof(struct bezier_handle));
    vector_init(&snake->bezier_points, sizeof(struct bezier_point));

    bezier_handle_init(vector_emplace(&snake->bezier_handles),
            make_qwpos(0, 0));
    bezier_handle_init(vector_emplace(&snake->bezier_handles),
            make_qwpos(0, 0));

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
snake_update_controls(struct snake* snake, const struct controls* controls)
{
    snake->controls = *controls;
}

/* ------------------------------------------------------------------------- */
void
snake_step(struct snake* snake, int sim_tick_rate)
{
    double a;
    double error_squared;
    qw dx, dy;
    qa angle_diff, target_angle;
    uint8_t target_speed;

    /*
     * The "controls" structure contains the absolute angle (in world space) of
     * the desired angle. That is, the angle from the snake's head to the mouse
     * cursor. It is stored in an unsigned char [0 .. 255]. We need to convert
     * it to radians [-pi .. pi) using the fixed point angle type "qa"
     */
    target_angle = make_qa(snake->controls.angle / 256.0 * 2*M_PI - M_PI);

    /* Calculate difference between desired angle and actual angle and wrap */
    angle_diff = qa_sub(snake->head_angle, target_angle);
    if (angle_diff > make_qa(M_PI))
        angle_diff = qa_sub(angle_diff, make_qa(2*M_PI));
    else if (angle_diff < make_qa(-M_PI))
        angle_diff = qa_add(angle_diff, make_qa(2*M_PI));

    /*
     * Turn the head towards the target angle and make sure to not exceed the
     * maximum turning speed.
     */
    if (angle_diff > TURN_SPEED)
        snake->head_angle = qa_sub(snake->head_angle, qa_mul(TURN_SPEED, make_qa2(sim_tick_rate, 60)));
    else if (angle_diff < -TURN_SPEED)
        snake->head_angle = qa_add(snake->head_angle, TURN_SPEED);
    else
        snake->head_angle = target_angle;

    /* Ensure the angle remains in the range [-pi .. pi) or bad things happen */
    if (snake->head_angle < make_qa(-M_PI))
        snake->head_angle = qa_add(snake->head_angle, make_qa(2*M_PI));
    else if (snake->head_angle >= make_qa(M_PI))
        snake->head_angle = qa_sub(snake->head_angle, make_qa(2*M_PI));

    /* Integrate speed over time */
    target_speed = snake->controls.boost ?
        255 :
        qw_sub(MAX_SPEED, MIN_SPEED) * snake->controls.speed / qw_sub(BOOST_SPEED, MIN_SPEED);
    if (snake->speed - target_speed > (ACCELERATION*255))
        snake->speed -= (ACCELERATION*255);
    else if (snake->speed - target_speed < (-ACCELERATION*255))
        snake->speed += (ACCELERATION*255);
    else
        snake->speed = target_speed;

    /* Update snake position using the head's current angle and speed */
    a = qa_to_float(snake->head_angle);
    dx = make_qw(cos(a) * (snake->speed / 255.0 * qw_to_float(BOOST_SPEED - MIN_SPEED) + qw_to_float(MIN_SPEED)));
    dy = make_qw(sin(a) * (snake->speed / 255.0 * qw_to_float(BOOST_SPEED - MIN_SPEED) + qw_to_float(MIN_SPEED)));
    snake->head_pos.x = qw_add(snake->head_pos.x, dx);
    snake->head_pos.y = qw_add(snake->head_pos.y, dy);

    /* Append new position to the list of points */
    vector_push(&snake->points, &snake->head_pos);

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
        vector_push(&snake->points, &snake->head_pos);
    }
}
