#pragma once

#include <stdint.h>

struct input;

enum cmd_action
{
    CMD_ACTION_NONE,
    CMD_ACTION_BOOST,
    CMD_ACTION_SHOOT,
    CMD_ACTION_REVERSE,
    CMD_ACTION_SPLIT,
    CMD_ACTION_GUARD
};

/*!
 * \brief Optimized for network traffic. This is the direct input for stepping
 * a snake forwards by 1 frame.
 */
struct cmd
{
    uint8_t  angle;
    uint8_t  speed;
    unsigned action : 3;
};

/*!
 * \brief Returns the default or fallback command. The snake is initialized with
 * this command by default, until the client changes it.
 */
struct cmd cmd_default(void);

/*!
 * \brief Constructs a new command given the previous command and parameters.
 *
 * Commands are constructed in a way to limit the number of bits necessary to
 * encode deltas. We do this by limiting the speed at which the "angle" and
 * "speed" properties can be updated from frame to frame. This function takes
 * care of the details.
 *
 * \param[in] prev The command from the previous frame
 * \param[in] radians The angle, in world space, to steer the snake towards
 * \param[in] speed A value between [0..1]. Here, 0 maps to the minimum speed
 * and 1 maps to the maximum (non-boost) speed. \param[in] action The current
 * action to perform.
 */
struct cmd cmd_make(
    struct cmd      prev,
    float           radians,
    float           normalized_speed,
    enum cmd_action action);

/*!
 * \brief Map user input into a "snake command structure", also known as a
 * "command frame".
 *
 * The command structure stores the world-space target angle of the snake
 * head as well as the target speed. These values are calculated from the
 * mouse position in screen space.
 *
 * \note Very important: Due to network optimizations, when calculating new
 * command you must limit the speed at which the "angle" and "speed" properties
 * are allowed to change. This limitation allows commands to be
 * delta-compressed more efficiently. cmd_make() takes care of this.
 *
 * \param[in] cmd The previously calculated command from the previous
 * frame.
 * \param[in] input Raw user input.
 * \return The new command. Make sure to use @see cmd_make()
 */
struct cmd cmd_next(struct cmd prev, const struct input* input);
