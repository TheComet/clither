#include "clither/tick.h"
#include <time.h>
#include <errno.h>

/* ------------------------------------------------------------------------- */
void
tick_init(struct tick* t, int tps)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t->last = ts.tv_sec * 1e9 + ts.tv_nsec;
    t->interval = 1e9 / tps;
}

/* ------------------------------------------------------------------------- */
int
tick_wait(struct tick* t)
{
    struct timespec ts;
    uint64_t now;
    uint64_t wait_until;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    now = ts.tv_sec * 1e9 + ts.tv_nsec;
    wait_until = t->last + t->interval;

    if (now > wait_until)
    {
        t->last = wait_until;
        return (now - wait_until) / t->interval + 1;
    }

    /* Context switch on linux takes about ~1us */
    if (wait_until - now > 2000)
    {
        ts.tv_sec = wait_until / 1e9;
        ts.tv_nsec = wait_until - ts.tv_sec * 1e9;
        if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == 0)
        {
            t->last = wait_until;
            return 0;
        }
    }

    do
    {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        now = ts.tv_sec * 1e9 + ts.tv_nsec;
    } while (now < wait_until);

    t->last = wait_until;
    return 0;
}

/* ------------------------------------------------------------------------- */
void
tick_skip(struct tick* t)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t->last = ts.tv_sec * 1e9 + ts.tv_nsec;
}
