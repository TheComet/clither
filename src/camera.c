#include "clither/camera.h"
#include "clither/snake.h"

#include <math.h>

/* ------------------------------------------------------------------------- */
void
camera_init(struct camera* camera)
{
    camera->pos = make_qwposi(0, 0);
    camera->scale = make_qw(1);
}

/* ------------------------------------------------------------------------- */
void
camera_update(struct camera* camera, const struct snake* snake, int sim_tick_rate)
{
    qw leadx = make_qw(cos(qa_to_float(snake->head.angle)) / 32);
    qw leady = make_qw(sin(qa_to_float(snake->head.angle)) / 32);
    qw targetx = qw_add(snake->head.pos.x, leadx);
    qw targety = qw_add(snake->head.pos.y, leady);
    qw dx = qw_mul(qw_sub(targetx, camera->pos.x), make_qw2(1, 8));
    qw dy = qw_mul(qw_sub(targety, camera->pos.y), make_qw2(1, 8));

    dx = qw_mul(dx, make_qw2(sim_tick_rate, 60));
    dy = qw_mul(dy, make_qw2(sim_tick_rate, 60));

    camera->pos.x = qw_add(camera->pos.x, dx);
    camera->pos.y = qw_add(camera->pos.y, dy);
}
