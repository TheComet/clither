#pragma once

#include "clither/config.h"
#include <stdint.h>

C_BEGIN

enum controls_action
{
    CONTROLS_ACTION_NONE,
    CONTROLS_ACTION_BOOST,
    CONTROLS_ACTION_SHOOT,
    CONTROLS_ACTION_REVERSE,
    CONTROLS_ACTION_SPLIT
};

/*!
 * \brief Maps directly to the user's mouse and button presses. This structure
 * is filled in by gfx_poll_input()
 */
struct input
{
    int mousex, mousey;
    unsigned boost : 1;
    unsigned quit  : 1;
};

/*!
 * \brief Optimized for network traffic. This is the direct input for stepping
 * a snake forwards by 1 frame.
 */
struct controls
{
    uint8_t angle;
    uint8_t speed;
    unsigned action : 3;
};

void
input_init(struct input* i);

void
controls_init(struct controls* c);

C_END
