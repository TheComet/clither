#include "clither/game/camera.h"
#include "clither/game/input.h"
#include "clither/game/snake.h"
#include <math.h>

/* ------------------------------------------------------------------------- */
void camera_init(struct camera* camera)
{
    camera->pos = make_qwposi(0, 0);
    camera->scale = make_qw(1);
#if defined(CLITHER_DEBUG_ZOOM)
    camera->debug_zoom = make_qw(1);
#endif
}

/* ------------------------------------------------------------------------- */
void camera_update(
    struct camera*            camera,
    const struct snake_head*  head,
    const struct snake_param* param,
    const struct input*       input,
    int                       sim_tick_rate)
{
    qw leadx = make_qw(cos(qa_to_float(head->angle)) / 32);
    qw leady = make_qw(sin(qa_to_float(head->angle)) / 32);
    qw targetx = qw_add(head->pos.x, leadx);
    qw targety = qw_add(head->pos.y, leady);
    qw dx = qw_mul(qw_sub(targetx, camera->pos.x), make_qw2(1, 4));
    qw dy = qw_mul(qw_sub(targety, camera->pos.y), make_qw2(1, 4));

    dx = qw_mul(dx, make_qw2(sim_tick_rate, 60));
    dy = qw_mul(dy, make_qw2(sim_tick_rate, 60));

    camera->pos.x = qw_add(camera->pos.x, dx);
    camera->pos.y = qw_add(camera->pos.y, dy);

    camera->scale = qw_div(make_qw(0.5), snake_scale(param));

#if defined(CLITHER_DEBUG_ZOOM)
    camera->debug_zoom += input->scroll * camera->debug_zoom * 0.05;
    camera->scale = qw_mul(camera->scale, camera->debug_zoom);
#else
    (void)input;
#endif
}
