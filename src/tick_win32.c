#include "clither/tick.h"
#include <Windows.h>

/* ------------------------------------------------------------------------- */
void
tick_cfg(struct tick* t, int tps)
{
    LARGE_INTEGER freq, ticks;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&ticks);

    t->last = ticks.QuadPart;
    t->interval = freq.QuadPart / tps;
}

/* ------------------------------------------------------------------------- */
int
tick_wait(struct tick* t)
{
    LARGE_INTEGER freq, ticks;
    uint64_t now, wait_until;
    QueryPerformanceCounter(&ticks);
    QueryPerformanceFrequency(&freq);
    now = ticks.QuadPart;
    wait_until = t->last + t->interval;

    if (now > wait_until)
    {
        t->last = wait_until;
        return (now - wait_until) / t->interval;
    }

    /* The best we can do for resolution on Windows is 1ms */
    if (wait_until - now > (uint64_t)freq.QuadPart / 1000)
    {
        Sleep((wait_until - now) / freq.QuadPart * 1000);
    }

    do
    {
        QueryPerformanceCounter(&ticks);
        now = ticks.QuadPart;
    } while (now < wait_until);

    t->last = wait_until;
    return 0;
}

/* ------------------------------------------------------------------------- */
void
tick_skip(struct tick* t)
{
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);
    t->last = ticks.QuadPart;
}
