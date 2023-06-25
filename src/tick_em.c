#include "clither/tick.h"

#include <emscripten.h>

/* ------------------------------------------------------------------------- */
void
tick_cfg(struct tick* t, int tps)
{
    t->last = (uint64_t)(emscripten_get_now() * 1e6);
    t->interval = 1e9 / tps;
}

/* ------------------------------------------------------------------------- */
int
tick_advance(struct tick* t)
{
    uint64_t now = (uint64_t)(emscripten_get_now() * 1e6);
    uint64_t wait_until = t->last + t->interval;

    if (now > wait_until)
    {
        int ticks_behind = (now - wait_until) / t->interval;
        if (ticks_behind >  0)
            t->last = wait_until;
        return ticks_behind;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
int
tick_wait(struct tick* t)
{
    unsigned int delta_ms;
    uint64_t now = (uint64_t)(emscripten_get_now() * 1e6);
    uint64_t wait_until = t->last + t->interval;

    if (now > wait_until)
    {
        t->last = wait_until;
        return (now - wait_until) / t->interval;
    }

    delta_ms = (unsigned int)((wait_until - now) / 1e6);
    emscripten_sleep(delta_ms);

    t->last = wait_until;
    return 0;
}

/* ------------------------------------------------------------------------- */
int
tick_wait_warp(struct tick* t, int warp, int tps)
{
    unsigned int delta_ms;
    uint64_t now = (uint64_t)(emscripten_get_now() * 1e6);
    uint64_t wait_until = t->last + t->interval;

    if (warp > 0)
        wait_until += 1e9 / tps;
    if (warp < 0)
        wait_until -= 1e9 / tps;

    if (now > wait_until)
    {
        t->last = wait_until;
        return (now - wait_until) / t->interval;
    }

    delta_ms = (unsigned int)((wait_until - now) / 1e6);
    emscripten_sleep(delta_ms);

    t->last = wait_until;
    return 0;
}

/* ------------------------------------------------------------------------- */
void
tick_skip(struct tick* t)
{
    t->last = (uint64_t)(emscripten_get_now() * 1e6);
}
