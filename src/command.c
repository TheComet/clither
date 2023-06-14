#include "clither/command.h"
#include "clither/log.h"
#include "clither/wrap.h"

#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>

/* ------------------------------------------------------------------------- */
struct command
command_default(void)
{
    struct command command;
    command.angle = 128;
    command.speed = 0;
    command.action = COMMAND_ACTION_NONE;
    return command;
}

/* ------------------------------------------------------------------------- */
struct command
command_make(
    struct command prev,
    float radians,
    float normalized_speed,
    enum command_action action)
{
    /* Yes, this 256 is not a mistake -- makes sure that not both of -3.141 and 3.141 are included */
    float a = radians / (2 * M_PI) + 0.5;
    uint8_t new_angle = (uint8_t)(a * 256);
    uint8_t new_speed = (uint8_t)(normalized_speed * 255);

    /*
     * The following code is designed to limit the number of bits necessary to
     * encode input deltas. The snake's turn speed is pretty slow, so we can
     * get away with 3 bits. Speed is a little more sensitive. Through testing,
     * 5 bits seems appropriate (see: snake.c, ACCELERATION is 8 per frame, so
     * we need at least 5 bits)
     */
    int da = new_angle - prev.angle;
    if (da > 128)
        da -= 256;
    if (da < -128)
        da += 256;
    if (da > 3)
        prev.angle += 3;
    else if (da < -3)
        prev.angle -= 3;
    else
        prev.angle = new_angle;

    /* (int) cast is necessary because msvc does not correctly deal with bitfields */
    if (new_speed - (int)prev.speed > 15)
        prev.speed += 15;
    else if (new_speed - (int)prev.speed < -15)
        prev.speed -= 15;
    else
        prev.speed = new_speed;

    prev.action = action;
    return prev;
}

/* ------------------------------------------------------------------------- */
void
command_rb_init(struct command_rb* rb)
{
    rb_init(&rb->rb, sizeof(struct command));
    rb->last_command_read = command_default();
    rb->first_frame = 0;
}

/* ------------------------------------------------------------------------- */
void
command_rb_deinit(struct command_rb* rb)
{
    rb_deinit(&rb->rb);
}

/* ------------------------------------------------------------------------- */
void
command_rb_put(
    struct command_rb* rb,
    struct command command,
    uint16_t frame_number)
{
    if (rb_count(&rb->rb) > 0)
    {
        uint16_t expected_frame = rb->first_frame + rb_count(&rb->rb);
        if (expected_frame != frame_number)
            return;
    }
    else
    {
        rb->first_frame = frame_number;
    }

    rb_put(&rb->rb, &command);
}

/* ------------------------------------------------------------------------- */
struct command
command_rb_take_or_predict(struct command_rb* rb, uint16_t frame_number)
{
    if (u16_lt_wrap(frame_number, command_rb_frame_begin(rb)))
        return rb->last_command_read;

    while (rb_count(&rb->rb) > 0)
    {
        uint16_t frame = command_rb_frame_begin(rb);
        rb->last_command_read = *(struct command*)rb_take(&rb->rb);
        rb->first_frame++;
        if (frame == frame_number)
            return rb->last_command_read;
    }

    return rb->last_command_read;
}

/* ------------------------------------------------------------------------- */
struct command
command_rb_find_or_predict(const struct command_rb* rb, uint16_t frame_number)
{
    uint16_t frame;

    if (u16_lt_wrap(frame_number, command_rb_frame_begin(rb)))
        return rb->last_command_read;

    frame = command_rb_frame_begin(rb);
    RB_FOR_EACH(&rb->rb, struct command, command)
        if (frame == frame_number)
            return *command;
        frame++;
    RB_END_EACH

    return rb_count(&rb->rb) == 0 ?
        rb->last_command_read :
        *(struct command*)rb_peek_write(&rb->rb);
}
