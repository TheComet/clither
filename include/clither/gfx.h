#pragma once

struct event_queue;

struct gfx
{
    struct {
        int mousex, mousey;
        unsigned boost : 1;
        unsigned quit  : 1;
    } input;
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
gfx_poll_input(struct gfx* g);

void
gfx_update(struct gfx* g);
