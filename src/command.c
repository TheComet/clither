#include "clither/command.h"
#include "clither/log.h"
#include "clither/wrap.h"

#include <string.h>

/* ------------------------------------------------------------------------- */
void
command_init(struct command* c)
{
    c->angle = 128;
    c->speed = 0;
    c->action = COMMAND_ACTION_NONE;
}

/* ------------------------------------------------------------------------- */
void
command_rb_init(struct command_rb* rb)
{
    rb_init(&rb->rb, sizeof(struct command));
    command_init(&rb->last_predicted);
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
    struct command* command;
    const uint16_t prev_frame = frame_number - 1;

    if (u16_lt_wrap(frame_number, command_rb_frame_begin(rb)))
        return rb->last_predicted;

    while (rb_count(&rb->rb) > 0)
    {
        command = rb_take(&rb->rb);
        rb->first_frame++;
        if (command_rb_frame_begin(rb) == prev_frame)
        {
            rb->last_predicted = *command;
            return *command;
        }
    }

    return rb->last_predicted;
}

/* ------------------------------------------------------------------------- */
struct command
command_rb_find_or_predict(const struct command_rb* rb, uint16_t frame_number)
{
    struct command* prev_command = NULL;
    uint16_t prev_frame = frame_number - 1;
    uint16_t frame = rb->first_frame;
    RB_FOR_EACH(&rb->rb, struct command, command)
        if (frame == prev_frame)
            prev_command = command;
        else if (frame == frame_number)
            return *command;
        frame++;
    RB_END_EACH

    if (prev_command == NULL)
        return rb->last_predicted;

    return *prev_command;
}
