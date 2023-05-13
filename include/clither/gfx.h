#pragma once

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
gfx_poll_input(struct gfx* g, struct input* c);

void
gfx_draw(struct gfx* g, struct input* c, struct world* w);
