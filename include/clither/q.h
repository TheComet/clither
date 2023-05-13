#pragma once

#include <stdint.h>

// precomputed value:
#define Q   10
#define K   (1 << (Q - 1))

/*
 * Since the world size is fixed, we use a Q19.5 (24-bit) fixed point representation
 * for world position.
 */
typedef int32_t q14_10;

struct qpos2
{
    q14_10 x;
    q14_10 y;
};

#define make_q(v) (q14_10)((v) * K)
#define make_qpos2(x, y) (struct qpos2){ make_q(x), make_q(y) }
#define q_to_int(q) (int)((q) / K)
#define q_to_float(q) ((double)(q) / K)

static inline q14_10 q_add(q14_10 a, q14_10 b)
{
    return a + b;
}

static inline q14_10 q_add_sat(q14_10 a, q14_10 b)
{
    q14_10 result;
    int64_t tmp;

    tmp = (int64_t)a + (int64_t)b;
    if (tmp > 0x7FFFFF)
        tmp = 0x7FFFFF;
    if (tmp < -1 * 0x800000)
        tmp = -1 * 0x800000;
    result = (q14_10)tmp;

    return result;
}

static inline q14_10 q_sub(q14_10 a, q14_10 b)
{
    return a - b;
}

// saturate to range of int16_t
static inline q14_10 sat16(int64_t x)
{
    if (x > 0x7FFFFF) return 0x7FFFFF;
    else if (x < -0x800000) return -0x800000;
    else return (q14_10)x;
}

static inline q14_10 q_mul(q14_10 a, q14_10 b)
{
    q14_10 result;
    int64_t temp;

    temp = (int64_t)a * (int64_t)b; // result type is operand's type
    // Rounding; mid values are rounded up
    temp += K;
    // Correct by dividing by base and saturate result
    result = sat16(temp >> Q);

    return result;
}

static inline q14_10 q_div(q14_10 a, q14_10 b)
{
    /* pre-multiply by the base (Upscale to Q32 so that the result will be in Q16 format) */
    int64_t temp = (int64_t)a << Q;
    /* Rounding: mid values are rounded up (down for negative values). */
    /* OR compare most significant bits i.e. if (((temp >> 31) & 1) == ((b >> 15) & 1)) */
    if ((temp >= 0 && b >= 0) || (temp < 0 && b < 0)) {
        temp += b / 2;    /* OR shift 1 bit i.e. temp += (b >> 1); */
    }
    else {
        temp -= b / 2;    /* OR shift 1 bit i.e. temp -= (b >> 1); */
    }
    return (q14_10)(temp / b);
}

#if 0
static inline int16_t q_add(int16_t a, int16_t b)
{
    return a + b;
}

static inline int16_t q_add_sat(int16_t a, int16_t b)
{
    int16_t result;
    int32_t tmp;

    tmp = (int32_t)a + (int32_t)b;
    if (tmp > 0x7FFF)
        tmp = 0x7FFF;
    if (tmp < -1 * 0x8000)
        tmp = -1 * 0x8000;
    result = (int16_t)tmp;

    return result;
}

static inline int16_t q_sub(int16_t a, int16_t b)
{
    return a - b;
}

// precomputed value:
#define K   (1 << (Q - 1))

// saturate to range of int16_t
static inline int16_t sat16(int32_t x)
{
    if (x > 0x7FFF) return 0x7FFF;
    else if (x < -0x8000) return -0x8000;
    else return (int16_t)x;
}

static inline int16_t q_mul(int16_t a, int16_t b)
{
    int16_t result;
    int32_t temp;

    temp = (int32_t)a * (int32_t)b; // result type is operand's type
    // Rounding; mid values are rounded up
    temp += K;
    // Correct by dividing by base and saturate result
    result = sat16(temp >> Q);

    return result;
}

static inline int16_t q_div(int16_t a, int16_t b)
{
    /* pre-multiply by the base (Upscale to Q16 so that the result will be in Q8 format) */
    int32_t temp = (int32_t)a << Q;
    /* Rounding: mid values are rounded up (down for negative values). */
    /* OR compare most significant bits i.e. if (((temp >> 31) & 1) == ((b >> 15) & 1)) */
    if ((temp >= 0 && b >= 0) || (temp < 0 && b < 0)) {
        temp += b / 2;    /* OR shift 1 bit i.e. temp += (b >> 1); */
    } else {
        temp -= b / 2;    /* OR shift 1 bit i.e. temp -= (b >> 1); */
    }
    return (int16_t)(temp / b);
}
#endif
