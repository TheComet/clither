#include "clither/command.h"
#include "clither/log.h"
#include "clither/wrap.h"
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>

/* ------------------------------------------------------------------------- */
struct command command_default(void)
{
    struct command command;
    command.angle = 128;
    command.speed = 0;
    command.action = COMMAND_ACTION_NONE;
    return command;
}

/* ------------------------------------------------------------------------- */
struct command command_make(
    struct command      prev,
    float               radians,
    float               normalized_speed,
    enum command_action action)
{
    /* Yes, this 256 is not a mistake -- makes sure that not both of -3.141
     * and 3.141 are included */
    float   a = radians / (2 * M_PI) + 0.5;
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

    /* (int) cast is necessary because msvc does not correctly deal with
     * bitfields */
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
void command_queue_init(struct command_queue* cmdq)
{
    command_rb_init(&cmdq->rb);
    cmdq->last_command_read = command_default();
    cmdq->first_frame = 0;
}

/* ------------------------------------------------------------------------- */
void command_queue_deinit(struct command_queue* cmdq)
{
    command_rb_deinit(cmdq->rb);
}

/* ------------------------------------------------------------------------- */
void command_queue_put(
    struct command_queue* cmdq, struct command command, uint16_t frame_number)
{
    if (rb_count(cmdq->rb) > 0)
    {
        uint16_t expected_frame = command_queue_frame_end(cmdq);
        if (expected_frame != frame_number)
            return;
    }
    else
    {
        cmdq->first_frame = frame_number;
    }

    command_rb_put(cmdq->rb, command);
}

/* ------------------------------------------------------------------------- */
struct command
command_queue_take_or_predict(struct command_queue* cmdq, uint16_t frame_number)
{
    if (u16_lt_wrap(frame_number, command_queue_frame_begin(cmdq)))
        return cmdq->last_command_read;

    while (rb_count(cmdq->rb) > 0)
    {
        uint16_t frame = command_queue_frame_begin(cmdq);
        cmdq->last_command_read = command_rb_take(cmdq->rb);
        cmdq->first_frame++;
        if (frame == frame_number)
            return cmdq->last_command_read;
    }

    log_dbg(
        "command_queue_take_or_predict(): No command for frame %d, "
        "predicting...\n",
        frame_number);
    return cmdq->last_command_read;
}

/* ------------------------------------------------------------------------- */
struct command command_queue_find_or_predict(
    const struct command_queue* crb, uint16_t frame_number)
{
    uint16_t              frame;
    int                   i;
    const struct command* command;

    if (u16_lt_wrap(frame_number, command_queue_frame_begin(crb)))
        return crb->last_command_read;

    frame = command_queue_frame_begin(crb);
    rb_for_each(crb->rb, i, command)
    {
        if (frame == frame_number)
            return *command;
        frame++;
    }

    log_dbg(
        "command_queue_find_or_predict(): No command for frame %d, "
        "predicting...\n",
        frame_number);
    return rb_count(crb->rb) == 0 ? crb->last_command_read
                                  : *rb_peek_write(crb->rb);
}
