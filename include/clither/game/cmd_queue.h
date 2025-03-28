#pragma once

#include "clither/cmd_rb.h"

struct cmd_queue
{
    struct cmd_rb* rb;
    struct cmd     last_command_read;
    uint16_t       first_frame;
};

void cmd_queue_init(struct cmd_queue* cmdq);

void cmd_queue_deinit(struct cmd_queue* cmdq);

/*!
 * \brief Inserts a new command into the ring buffer.
 * \param[in] rb Ring buffer.
 * \param[in] command Command to insert
 * \param[in] frame_number A snake on frame N-1 can be stepped forwards to frame
 * N with a command from frame N. In other words, this is the "target frame" or
 * "post simulation frame".
 * \note The ring buffer will only ever insert commands if the frame number is
 * one after the previously inserted command. If the ring buffer is empty then
 * inserting always works.
 */
void cmd_queue_put(
    struct cmd_queue* cmdq, struct cmd command, uint16_t frame_number);

/*!
 * \brief Takes the command with the requested frame_number from the ring buffer
 * and returns it. All commands predating the specified frame_number are also
 * removed from the buffer. If the ring buffer is empty, or becomes empty, then
 * the last command taking from the buffer is returned instead (duplication of
 * last command).
 */
struct cmd
cmd_queue_take_or_predict(struct cmd_queue* cmdq, uint16_t frame_number);

/*!
 * \brief Finds the command with the requested frame_number and returns it. If
 * no such command exists, then the last command inserted is returned instead
 * (duplication of last command). This function does not modify the buffer,
 * unlike command_queue_take_or_predict().
 */
struct cmd
cmd_queue_find_or_predict(const struct cmd_queue* cmdq, uint16_t frame_number);

/*! \brief Returns the number of commands in the buffer */
#define cmd_queue_count(cmdq) rb_count((cmdq)->rb)
/*! \brief Returns the frame_number of the oldest command in the buffer */
#define cmd_queue_frame_begin(cmdq) ((cmdq)->first_frame)
/*! \brief Returns frame_number+1 of the newest command in the buffer */
#define cmd_queue_frame_end(cmdq)                                              \
    (uint16_t)((cmdq)->first_frame + rb_count((cmdq)->rb))
/*! \brief Return a command at an index. Range is 0 to command_queue_count()-1
 */
#define cmd_queue_peek(cmdq, idx) (rb_peek((cmdq)->rb, idx))
/*! \brief Clear all commands */
#define cmd_queue_clear(cmdq) cmd_rb_clear((cmdq)->rb)

#define cmd_queue_for_each(cmdq, i, frame_var, command_var)                    \
    for (i = (cmdq)->rb ? (cmdq)->rb->read : 0,                                \
        frame_var = (cmdq)->first_frame - 1;                                   \
         i != ((cmdq)->rb ? (cmdq)->rb->write : 0) &&                          \
         ((command_var = &(cmdq)->rb->data[i]) || 1);                          \
         (i = (i + 1) & ((cmdq)->rb->capacity - 1)), frame_var++)
