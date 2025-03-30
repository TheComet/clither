#pragma once

#include "clither/config.h"
#include "clither/game/q.h"

struct snake_head;
struct snake_param;
struct input;

struct camera
{
    struct qwpos pos;
    qw           scale;
#if defined(CLITHER_DEBUG_ZOOM)
    qw debug_zoom;
#endif
};

void camera_init(struct camera* camera);

void camera_update(
    struct camera*            camera,
    const struct snake_head*  head,
    const struct snake_param* param,
    const struct input*       input,
    int                       sim_tick_rate);
