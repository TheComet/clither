#pragma once

#include "clither/util/vec.h"
#include <stdint.h>

VEC_DECLARE(codepoint_vec, uint32_t, 8)

/*!
 * \brief Maps directly to the user's mouse and button presses. This structure
 * is filled in by gfx_poll_input()
 */
struct input
{
    struct codepoint_vec* keys;
    /* Mouse coordinates with aspect ratio correction. One axis will go over/
     * under [-1.0, 1.0] */
    float mousex_ar, mousey_ar;
    /* Normalized mouse coordinates [-1.0, 1.0] */
    float mousex, mousey;

    int16_t scroll; /* Mouse wheel (difference) */

    unsigned boost : 1; /* Boost button is pressed */
    unsigned shoot : 1; /* Shoot button is pressed */
    unsigned split : 1; /* Split snake button is pressed */

    unsigned mouse_moved : 1;
    unsigned mouse_down : 1;
    unsigned screen_clicked : 1; /* User clicked on the screen -- used for UI */
    unsigned backspace : 1;      /* User pressed the backspace key */
    unsigned enter : 1;          /* Enter key */
    unsigned escape : 1;         /* Escape key */
    unsigned quit : 1;           /* User pressed escape or similar */

    unsigned next_gfx_backend : 1; /* User pressed the key for switching to the
                                      next graphics backend*/
    unsigned prev_gfx_backend : 1; /* User pressed the key for switching to the
                                      previous graphics backend */
    unsigned debug_gfx : 1;        /* User pressed the key for
                                      toggling debug graphics */
};

void input_init(struct input* i);
void input_deinit(struct input* i);

void input_set_and_clear(struct input* i1, struct input* i2);
