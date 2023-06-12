#pragma once

#include "clither/config.h"
#include "clither/q.h"

struct camera;
struct controls;
struct input;
struct world;

struct gfx;

int
gfx_init(void);

void
gfx_deinit(void);

struct gfx*
gfx_create(int initial_width, int initial_height);

void
gfx_destroy(struct gfx* gfx);

void
gfx_poll_input(struct gfx* gfx, struct input* input);

void
gfx_update_controls(
    struct controls* controls,
    const struct input* input,
    const struct gfx* gfx,
    const struct camera* cam,
    struct qwpos snake_head);

void
gfx_draw_world(struct gfx* gfx, const struct world* world, const struct camera* camera);
