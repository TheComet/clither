#pragma once

#include "clither/config.h"
#include <stdint.h>

/*!
 * \brief Maps directly to the user's mouse and button presses. This structure
 * is filled in by gfx_poll_input()
 */
struct input
{
    /* Normalized mouse coordinates [-1.0, 1.0] */
    float mousex, mousey;
    /* Mouse coordinates with aspect ratio correction. One axis will go over/
     * under [-1.0, 1.0] */
    float mousex_ar, mousey_ar;

    int16_t  scroll; /* Mouse wheel (difference) */
    unsigned menu_clicked : 1;
    unsigned boost : 1;            /* Boost button is pressed */
    unsigned shoot : 1;            /* Shoot button is pressed */
    unsigned split : 1;            /* Split snake button is pressed */
    unsigned quit : 1;             /* User pressed escape or similar */
    unsigned next_gfx_backend : 1; /* User pressed the key for switching to the
                                      next graphics backend*/
    unsigned prev_gfx_backend : 1; /* User pressed the key for switching to the
                                      previous graphics backend */
    unsigned debug_gfx : 1;        /* User pressed the key for
                                      toggling debug graphics */
};

void input_init(struct input* i);
