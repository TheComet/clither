#pragma once

#include "clither/config.h"
#include "clither/q.h"

struct snake_head;
struct snake_param;

struct camera
{
    struct qwpos pos;
    qw scale;
};

void
camera_init(struct camera* camera);

void
camera_update(
    struct camera* camera,
    const struct snake_head* head,
    const struct snake_param* param,
    int sim_tick_rate);
