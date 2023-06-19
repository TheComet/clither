#include "clither/tick.h"

#include <emscripten.h>

void
tick_cfg(struct tick* t, int tps)
{
}

int
tick_advance(struct tick* t)
{
    emscripten_get_now();
    return 0;
}

int
tick_wait(struct tick* t)
{
    return 0;
}

int
tick_wait_warp(struct tick* t, int warp, int tps)
{
    return 0;
}

void
tick_skip(struct tick* t)
{
}

