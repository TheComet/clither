#pragma once

#include "clither/config.h"
#include "clither/q.h"
#include "cstructures/rb.h"
#include <stdint.h>

C_BEGIN

enum command_action
{
    COMMAND_ACTION_NONE,
    COMMAND_ACTION_BOOST,
    COMMAND_ACTION_SHOOT,
    COMMAND_ACTION_REVERSE,
    COMMAND_ACTION_SPLIT,
    COMMAND_ACTION_GUARD
};

/*!
 * \brief Optimized for network traffic. This is the direct input for stepping
 * a snake forwards by 1 frame.
 */
struct command
{
    uint8_t angle;
    uint8_t speed;
    unsigned action : 3;
};

struct command_rb
{
    struct cs_rb rb;
    struct command last_command_read;
    uint16_t first_frame;
};

/*!
 * \brief Returns the default or fallback command. The snake is initialized with
 * this command by default, until the client changes it.
 */
struct command
command_default(void);

/*!
 * \brief Constructs a new command given the previous command and parameters.
 * 
 * Commands are constructed in a way to limit the number of bits necessary to
 * encode deltas. We do this by limiting the speed at which the "angle" and "speed"
 * properties can be updated from frame to frame. This function takes care of
 * the details.
 * 
 * \param[in] prev The command from the previous frame
 * \param[in] radians The angle, in world space, to steer the snake towards
 * \param[in] speed A value between [0..1]. Here, 0 maps to the minimum speed and
 * 1 maps to the maximum (non-boost) speed.
 * \param[in] action The current action to perform.
 */
struct command
command_make(
    struct command prev,
    float radians,
    float normalized_speed,
    enum command_action action);

void
command_rb_init(struct command_rb* crb);

void
command_rb_deinit(struct command_rb* crb);

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
void
command_rb_put(
    struct command_rb* crb,
    struct command command,
    uint16_t frame_number);

/*!
 * \brief Takes the command with the requested frame_number from the ring buffer
 * and returns it. All commands predating the specified frame_number are also
 * removed from the buffer. If the ring buffer is empty, or becomes empty, then
 * the last command taking from the buffer is returned instead (duplication of
 * last command).
 */
struct command
command_rb_take_or_predict(
    struct command_rb* crb,
    uint16_t frame_number);

/*!
 * \brief Finds the command with the requested frame_number and returns it. If
 * no such command exists, then the last command inserted is returned instead
 * (duplication of last command). This function does not modify the buffer,
 * unlike command_rb_take_or_predict().
 */
struct command
command_rb_find_or_predict(
    const struct command_rb* crb,
    uint16_t frame_number);

/*! \brief Returns the number of commands in the buffer */
#define command_rb_count(crb) rb_count(&(crb)->rb)
/*! \brief Returns the frame_number of the oldest command in the buffer */
#define command_rb_frame_begin(crb) ((crb)->first_frame)
/*! \brief Returns frame_number+1 of the newest command in the buffer */
#define command_rb_frame_end(crb) (uint16_t)((crb)->first_frame + rb_count(&(crb)->rb))
/*! \brief Return a command at an index. Range is 0 to command_rb_count()-1 */
#define command_rb_peek(crb, idx) ((struct command*)rb_peek(&(crb)->rb, idx))
/*! \brief Clear all commands */
#define command_rb_clear(crb) rb_clear(&(crb)->rb)

/*! \brief Converts a commands angle attribute into fixed-point radians (@see qa) */
#define command_angle_to_qa(command) \
    (qa_rescale(2*QA_PI, command.angle, 256) - QA_PI)

#define COMMAND_RB_FOR_EACH(crb, frame_var, command_var) { \
    uint16_t frame_var = (crb)->first_frame - 1;             \
    RB_FOR_EACH(&(crb)->rb, struct command, command_var)   \
        frame_var++; {

#define COMMAND_RB_END_EACH \
    }} RB_END_EACH

C_END
