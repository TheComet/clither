#include "clither/game/input.h"
#include <string.h>

/* ------------------------------------------------------------------------- */
void input_init(struct input* i)
{
    memset(i, 0, sizeof *i);
    i->mousex = -1;
    i->mousey = -1;
    i->mousex_ar = -1;
    i->mousey_ar = -1;
}
