#include "clither/controls.h"
#include <string.h>

/* ------------------------------------------------------------------------- */
void
input_init(struct input* i)
{
    memset(i, 0, sizeof *i);
}

/* ------------------------------------------------------------------------- */
void
controls_init(struct controls* c, uint16_t frame_number)
{
    c->frame_number = frame_number;
    c->angle = 0;
    c->speed = 0;
    c->action = CONTROLS_ACTION_NONE;
}
