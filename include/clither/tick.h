#pragma once

#include "clither/config.h"
#include <stdint.h>

C_BEGIN

struct tick
{
    uint64_t interval;
    uint64_t last;
};

/*!
 * \brief Configures the "tick rate" and grabs the current time, so that the
 * next call to tick_wait() will wait the proper amount of time. Make sure to
 * call this function before you use tick_wait(), or if you wait a long period
 * of time between tick_wait() calls.
 * \param[in] t Tick structure
 * \param[in] tps Ticks per second. Configures how long tick_wait() will wait
 * for to achieve the number of ticks per second.
 */
void
tick_cfg(struct tick* t, int tps);

/*!
 * \brief If the period of time configured in tick_cfg() has passed since the
 * last call to tick_advance(), this function returns non-zero. This is
 * essentially a non-blocking version of tick_wait().
 * \return Returns the number of periods (configured in tick_cfg()) that you
 * are behind from where you should be.
 */
int
tick_advance(struct tick* t);

/*!
 * \brief Waits (and sleeps if necessary) until the tick rate configured in
 * tick_init() is reached. Then updates the current time, so that the next
 * call to tick_wait() will delay for the same amount of time again.
 * \param[in] t Tick structure
 * \return If a long amount of time has passed between calls to tick_wait(),
 * then this function will return the number of ticks that you are behind from
 * where you should be. Consider checking this value and setting a threshold,
 * and if you ever go over this threshold it might make sense to call tick_init()
 * again or tick_skip(). Will return 0 under normal circumstances.
 */
int
tick_wait(struct tick* t);

int
tick_wait_warp(struct tick* t, int warp, int tps);

void
tick_skip(struct tick* t);

C_END
