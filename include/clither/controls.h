#pragma once

#include "clither/config.h"

C_BEGIN

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
    unsigned angle   : 8;
    unsigned speed   : 8;
    unsigned boost   : 1;
    unsigned shoot   : 1;
    unsigned reverse : 1;
};

void
input_init(struct input* i);

void
controls_init(struct controls* c);

C_END
