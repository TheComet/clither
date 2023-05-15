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
controls_init(struct controls* c)
{
    memset(c, 0, sizeof *c);
}
