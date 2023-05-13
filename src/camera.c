#include "clither/camera.h"

/* ------------------------------------------------------------------------- */
void
camera_init(struct camera* c)
{
    c->pos = make_qpos2(0, 0);
    c->scale = make_q(1);
}
