#pragma once

#include "clither/config.h"

C_BEGIN

/*!
 * \brief Maps directly to the user's mouse and button presses. This structure
 * is filled in by gfx_poll_input()
 */
struct input
{
    int mousex, mousey;  /* Mouse position in screen coordinates */
    int scroll;          /* Mouse wheel (difference) */
    unsigned boost : 1;  /* Boost button is pressed */
    unsigned shoot : 1;
    unsigned split : 1;
    unsigned quit  : 1;  /* User pressed escape or similar */
};

void
input_init(struct input* i);

C_END
