#include "clither/bezier.h"
#include "clither/log.h"
#include "clither/q.h"
#include "clither/snake.h"

#include "cstructures/memory.h"

#define _USE_MATH_DEFINES
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define TURN_SPEED   (1.0 / 16)
#define MIN_SPEED    (1.0 / 512)
#define MAX_SPEED    (1.0 / 96)
#define BOOST_SPEED  (1.0 / 32)
#define ACCELERATION (1.0 / 32)

/* ------------------------------------------------------------------------- */
void
snake_init(struct snake* snake, const char* name)
{
    snake->name = MALLOC(strlen(name) + 1);
    strcpy(snake->name, name);

    controls_init(&snake->controls);

    snake->head_pos = make_qwpos2(0, 0);
    snake->head_angle = make_qa(0);
    snake->speed = 0;

    vector_init(&snake->points, sizeof(struct qwpos2));
    vector_init(&snake->bezier_handles, sizeof(struct bezier_handle));

    bezier_handle_init(vector_emplace(&snake->bezier_handles),
            make_qwpos2(0, 0), 0, 50);
    bezier_handle_init(vector_emplace(&snake->bezier_handles),
            make_qwpos2(0, 0), 128, 50);

    snake->length = 50;
}

/* ------------------------------------------------------------------------- */
void
snake_deinit(struct snake* snake)
{
    FREE(snake->name);
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
     * Turn the head towards the target angle and make sure to not go over the
     * maximum turning speed
     */
    if (angle_diff > make_qa(TURN_SPEED))
        snake->head_angle = qa_sub(snake->head_angle, make_qa(TURN_SPEED));
    else if (angle_diff < make_qa(-TURN_SPEED))
        snake->head_angle = qa_add(snake->head_angle, make_qa(TURN_SPEED));
    else
        snake->head_angle = target_angle;

    /* Ensure the angle remains in the range [-pi .. pi) or bad things happen */
    if (snake->head_angle < make_qa(-M_PI))
        snake->head_angle = qa_add(snake->head_angle, make_qa(2*M_PI));
    else if (snake->head_angle >= make_qa(M_PI))
        snake->head_angle = qa_sub(snake->head_angle, make_qa(2*M_PI));

    while (vector_count(&snake->points) >= snake->length)
        vector_pop(&snake->points);
    vector_insert(&snake->points, 0, &snake->head_pos);

    {
        struct bezier_handle* bh = vector_front(&snake->bezier_handles);
        struct qwpos2 p = *(struct qwpos2*)vector_front(&snake->points);
        bh->pos = p;
    }
    if (vector_count(&snake->points) == snake->length)
    {
        qw dx, dy;
        struct qwpos2 p1 = *(struct qwpos2*)vector_get_element(&snake->points, vector_count(&snake->points) - 1);
        struct qwpos2 p2 = *(struct qwpos2*)vector_get_element(&snake->points, vector_count(&snake->points) - 2);
        struct bezier_handle* bh = vector_get_element(&snake->bezier_handles, 1);

        dx = qw_sub(p1.x, p2.x);
        dy = qw_sub(p1.y, p2.y);
        bh->pos = p1;
        bh->angle = make_qa(atan2(dy, dx));
    }

    target_speed = snake->controls.boost ?
        255 :
        make_qw(MAX_SPEED - MIN_SPEED) * snake->controls.speed / make_qw(BOOST_SPEED - MIN_SPEED);
    if (snake->speed - target_speed > (ACCELERATION*255))
        snake->speed -= (ACCELERATION*255);
    else if (snake->speed - target_speed < (-ACCELERATION*255))
        snake->speed += (ACCELERATION*255);
    else
        snake->speed = target_speed;

    /* Update snake position using the head's current angle */
    a = qa_to_float(snake->head_angle);
    dx = make_qw(cos(a) * (snake->speed / 255.0 * (BOOST_SPEED - MIN_SPEED) + MIN_SPEED));
    dy = make_qw(sin(a) * (snake->speed / 255.0 * (BOOST_SPEED - MIN_SPEED) + MIN_SPEED));
    snake->head_pos.x = qw_add(snake->head_pos.x, dx);
    snake->head_pos.y = qw_add(snake->head_pos.y, dy);
}
