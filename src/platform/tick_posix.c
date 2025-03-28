#include "clither/tick.h"
#include <errno.h>
#include <time.h>

/* ------------------------------------------------------------------------- */
void tick_cfg(struct tick* t, int tps)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t->last = ts.tv_sec * 1000000000 + ts.tv_nsec;
    t->interval = 1000000000 / tps;
}

/* ------------------------------------------------------------------------- */
int tick_advance(struct tick* t)
{
    struct timespec ts;
    uint64_t        now;
    uint64_t        wait_until;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    now = ts.tv_sec * 1000000000 + ts.tv_nsec;
    wait_until = t->last + t->interval;

    if (now > wait_until)
    {
        int ticks_behind = (now - wait_until) / t->interval;
        if (ticks_behind > 0)
            t->last = wait_until;
        return ticks_behind;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
int tick_wait(struct tick* t)
{
    struct timespec ts;
    uint64_t        now;
    uint64_t        wait_until;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    now = ts.tv_sec * 1000000000 + ts.tv_nsec;
    wait_until = t->last + t->interval;

    if (now > wait_until)
    {
        t->last = wait_until;
        return (now - wait_until) / t->interval;
    }

    /* Context switch on linux takes about ~1us */
    if (wait_until - now > 2000)
    {
        ts.tv_sec = wait_until / 1000000000;
        ts.tv_nsec = wait_until - ts.tv_sec * 1000000000;
        if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == 0)
        {
            t->last = wait_until;
            return 0;
        }
    }

    do
    {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        now = ts.tv_sec * 1000000000 + ts.tv_nsec;
    } while (now < wait_until);

    t->last = wait_until;
    return 0;
}

/* ------------------------------------------------------------------------- */
int tick_wait_warp(struct tick* t, int warp, int tps)
{
    struct timespec ts;
    uint64_t        now;
    uint64_t        wait_until;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    now = ts.tv_sec * 1000000000 + ts.tv_nsec;
    wait_until = t->last + t->interval;

    if (warp > 0)
        wait_until += 1000000000 / tps;
    if (warp < 0)
        wait_until -= 1000000000 / tps;

    if (now > wait_until)
    {
        t->last = wait_until;
        return (now - wait_until) / t->interval;
    }

    /* Context switch on linux takes about ~1us */
    if (wait_until - now > 2000)
    {
        ts.tv_sec = wait_until / 1000000000;
        ts.tv_nsec = wait_until - ts.tv_sec * 1000000000;
        if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == 0)
        {
            t->last = wait_until;
            return 0;
        }
    }

    do
    {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        now = ts.tv_sec * 1000000000 + ts.tv_nsec;
    } while (now < wait_until);

    t->last = wait_until;
    return 0;
}

/* ------------------------------------------------------------------------- */
void tick_skip(struct tick* t)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t->last = ts.tv_sec * 1000000000 + ts.tv_nsec;
}
