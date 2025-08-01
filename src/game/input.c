#include "clither/game/input.h"
#include <string.h>

VEC_DEFINE(codepoint_vec, uint32_t, 8)

/* ------------------------------------------------------------------------- */
void input_init(struct input* i)
{
    memset(i, 0, sizeof *i);
    codepoint_vec_init(&i->keys);

    i->mousex = -1.0;
    i->mousey = -1.0;
    i->mousex_ar = -1.0;
    i->mousey_ar = -1.0;
}

/* ------------------------------------------------------------------------- */
void input_deinit(struct input* i)
{
    codepoint_vec_deinit(i->keys);
}

/* ------------------------------------------------------------------------- */
void input_set_and_clear(struct input* i1, struct input* i2)
{
    struct codepoint_vec* tmp = i1->keys;
    *i1 = *i2;
    i2->keys = tmp;

    i2->mouse_moved = 0;
    i2->scroll = 0;
    i2->screen_clicked = 0;
    i2->backspace = 0;
    i2->escape = 0;
    i2->enter = 0;
    codepoint_vec_clear(i2->keys);
}
