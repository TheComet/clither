#pragma once

#include <stdint.h>

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

