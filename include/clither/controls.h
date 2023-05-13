#pragma once

#include "clither/q.h"

/*
 * Maps directly to the user's mouse and button presses. This structure is
 * filled in by gfx_poll_input()
 */
struct input
{
    int mousex, mousey;
    unsigned boost : 1;
    unsigned quit : 1;
};


struct controls
{
    unsigned angle    : 8;
    unsigned distance : 8;
};
