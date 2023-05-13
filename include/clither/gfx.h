#pragma once

struct camera;
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
gfx_destroy(struct gfx* g);

void
gfx_poll_input(struct gfx* g, struct input* i);

struct qpos2
gfx_screen_to_world(struct gfx* g, const struct camera* c, int x, int y);

void
gfx_draw_world(struct gfx* g, const struct world* w, const struct camera* c, const struct input* i);
