#pragma once

#include "clither/config.h"

C_BEGIN

enum controls_action
{
    CONTROLS_ACTION_NONE,
    CONTROLS_ACTION_BOOST,
    CONTROLS_ACTION_SHOOT,
    CONTROLS_ACTION_REVERSE,
    CONTROLS_ACTION_SPLIT
};

/*
 * Maps directly to the user's mouse and button presses. This structure is
 * filled in by gfx_poll_input()
 */
struct input
{
    int mousex, mousey;
    unsigned boost : 1;
    unsigned quit  : 1;
};

struct controls
{
    unsigned angle  : 8;
    unsigned speed  : 8;
    unsigned action : 3;
};

void
input_init(struct input* i);

void
controls_init(struct controls* c);

C_END
