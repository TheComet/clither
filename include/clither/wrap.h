#pragma once

#include <stdint.h>

static inline int
u8_gt_wrap(uint8_t a, uint8_t b)
{
    if ((int)a - (int)b > 0x7F)
        return 0;
    if ((int)a - (int)b < -0x7F)
        return 1;
    return a > b;
}

static inline int
u8_ge_wrap(uint8_t a, uint8_t b)
{
    if ((int)a - (int)b > 0x7F)
        return 0;
    if ((int)a - (int)b < -0x7F)
        return 1;
    return a >= b;
}

static inline int
u16_gt_wrap(uint16_t a, uint16_t b)
{
    if ((int)a - (int)b > 0x7FFF)
        return 0;
    if ((int)a - (int)b < -0x7FFF)
        return 1;
    return a > b;
}

static inline int
u16_ge_wrap(uint16_t a, uint16_t b)
{
    if ((int)a - (int)b > 0x7FFF)
        return 0;
    if ((int)a - (int)b < -0x7FFF)
        return 1;
    return a >= b;
}

static inline int
u16_lt_wrap(uint16_t a, uint16_t b) { return !u16_ge_wrap(a, b); }

static inline int
u16_le_wrap(uint16_t a, uint16_t b) { return !u16_gt_wrap(a, b); }

static inline int16_t
u16_sub_wrap(uint16_t a, uint16_t b)
{
    return a - b;
}
