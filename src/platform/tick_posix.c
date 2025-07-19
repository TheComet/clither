#include "clither/platform/tick.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_DEBUG)
int is_debugger_present(void)
{
    char  line[256];
    int   tracer_pid = 0;
    FILE* f = fopen("/proc/self/status", "r");
    if (f == NULL)
        return 0;

    while (fgets(line, sizeof(line), f))
        if (strncmp(line, "TracerPid:", 10) == 0)
        {
            tracer_pid = atoi(line + 10);
            break;
        }

    fclose(f);
    return tracer_pid != 0;
}
#endif

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
        int ticks_behind = (now - wait_until) / t->interval;
        t->last = wait_until;
#if defined(CLITHER_DEBUG)
        if (ticks_behind > 10 && is_debugger_present())
        {
            t->last = now;
            ticks_behind = 0;
        }
#endif
        return ticks_behind;
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
        int ticks_behind = (now - wait_until) / t->interval;
        t->last = wait_until;
#if defined(CLITHER_DEBUG)
        if (ticks_behind > 10 && is_debugger_present())
        {
            t->last = now;
            ticks_behind = 0;
        }
#endif
        return ticks_behind;
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
void tick_warp(struct tick* t, int warp, int tps)
{
    if (warp > 0)
        t->last += 1000000000 / tps;
    if (warp < 0)
        t->last -= 1000000000 / tps;
}

/* ------------------------------------------------------------------------- */
void tick_skip(struct tick* t)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t->last = ts.tv_sec * 1000000000 + ts.tv_nsec;
}
