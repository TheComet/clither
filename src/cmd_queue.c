#include "clither/cmd_queue.h"
#include "clither/wrap.h"

/* ------------------------------------------------------------------------- */
void cmd_queue_init(struct cmd_queue* cmdq)
{
    cmd_rb_init(&cmdq->rb);
    cmdq->last_command_read = cmd_default();
    cmdq->first_frame = 0;
}

/* ------------------------------------------------------------------------- */
void cmd_queue_deinit(struct cmd_queue* cmdq)
{
    cmd_rb_deinit(cmdq->rb);
}

/* ------------------------------------------------------------------------- */
void cmd_queue_put(
    struct cmd_queue* cmdq, struct cmd command, uint16_t frame_number)
{
    if (rb_count(cmdq->rb) > 0)
    {
        uint16_t expected_frame = cmd_queue_frame_end(cmdq);
        if (expected_frame != frame_number)
            return;
    }
    else
    {
        cmdq->first_frame = frame_number;
    }

    cmd_rb_put_realloc(&cmdq->rb, command);
}

/* ------------------------------------------------------------------------- */
struct cmd
cmd_queue_take_or_predict(struct cmd_queue* cmdq, uint16_t frame_number)
{
    if (u16_lt_wrap(frame_number, cmd_queue_frame_begin(cmdq)))
        return cmdq->last_command_read;

    while (rb_count(cmdq->rb) > 0)
    {
        uint16_t frame = cmd_queue_frame_begin(cmdq);
        cmdq->last_command_read = cmd_rb_take(cmdq->rb);
        cmdq->first_frame++;
        if (frame == frame_number)
            return cmdq->last_command_read;
    }

    log_dbg(
        "cmd_queue_take_or_predict(): No command for frame %d, "
        "predicting...\n",
        frame_number);
    return cmdq->last_command_read;
}

/* ------------------------------------------------------------------------- */
struct cmd
cmd_queue_find_or_predict(const struct cmd_queue* crb, uint16_t frame_number)
{
    uint16_t          frame;
    int               i;
    const struct cmd* command;

    if (u16_lt_wrap(frame_number, cmd_queue_frame_begin(crb)))
        return crb->last_command_read;

    frame = cmd_queue_frame_begin(crb);
    rb_for_each (crb->rb, i, command)
    {
        if (frame == frame_number)
            return *command;
        frame++;
    }

    log_dbg(
        "cmd_queue_find_or_predict(): No command for frame %d, "
        "predicting...\n",
        frame_number);
    return rb_count(crb->rb) == 0 ? crb->last_command_read
                                  : *rb_peek_write(crb->rb);
}
