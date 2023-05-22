#include "clither/camera.h"

/* ------------------------------------------------------------------------- */
void
camera_init(struct camera* c)
{
    c->pos = make_qwposi(0, 0);
    c->scale = make_qw(1);
}
