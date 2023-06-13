#include "clither/input.h"
#include <string.h>

/* ------------------------------------------------------------------------- */
void
input_init(struct input* i)
{
    memset(i, 0, sizeof *i);
}
