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
#define MAX_SPEED    (1.0 / 128)
#define BOOST_SPEED  (1.0 / 32)

/* ------------------------------------------------------------------------- */
void
snake_init(struct snake* snake, const char* name)
{
    snake->name = MALLOC(strlen(name) + 1);
    strcpy(snake->name, name);

    controls_init(&snake->controls);

    snake->head_pos = make_qwpos2(0, 0);
    snake->head_angle = make_qa(0);

    vector_init(&snake->points, sizeof(struct qwpos2));
    vector_init(&snake->bezier_handles, sizeof(struct bezier_handle));

    snake->length = 20;
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
    double a, speed;
    qw dx, dy;
    qa angle_diff, target_angle;

    /*
     * The "controls" structure contains the absolute angle (in world space) of
     * the desired angle. That is, the angle from the snake's head to the mouse
     * cursor. It is stored in an unsigned char [0 .. 255]. We need to convert
     * it to radians [-pi .. pi] using the fixed point angle type "qa"
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

    /* Ensure the angle remains in the range [-pi .. pi] or bad things happen */
    if (snake->head_angle < make_qa(-M_PI))
        snake->head_angle = qa_add(snake->head_angle, make_qa(2*M_PI));
    else if (snake->head_angle >= make_qa(M_PI))
        snake->head_angle = qa_sub(snake->head_angle, make_qa(2*M_PI));

    /* Update snake position using the head's current angle */
    speed = snake->controls.boost ?
        BOOST_SPEED :
        (snake->controls.speed / 255.0 * (MAX_SPEED - MIN_SPEED) + MIN_SPEED);
    a = qa_to_float(snake->head_angle);
    dx = make_qw(cos(a) * speed);
    dy = make_qw(sin(a) * speed);
    snake->head_pos.x = qw_add(snake->head_pos.x, dx);
    snake->head_pos.y = qw_add(snake->head_pos.y, dy);

    log_dbg("pos: %f, %f\n", qw_to_float(snake->head_pos.x), qw_to_float(snake->head_pos.y));
}
