#pragma once

#include "clither/config.h"
#include "clither/q.h"

struct camera;
struct controls;
struct input;
struct world;

struct gfx
{
    char unused;
};

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
gfx_calc_controls(
    struct controls* controls,
    const struct input* input,
    const struct gfx* gfx,
    const struct camera* cam,
    struct qwpos2 snake_head);

struct qwpos2
gfx_screen_to_world(struct ipos2 pos, const struct gfx* gfx, const struct camera* camera);

struct ipos2
gfx_world_to_screen(struct qwpos2 pos, const struct gfx* gfx, const struct camera* camera);

void
gfx_draw_world(struct gfx* gfx, const struct world* world, const struct camera* camera);
