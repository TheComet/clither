#pragma once

#include <stdint.h>

// precomputed value:


/*
 * Since the world size is fixed, we use a Q19.5 (24-bit) fixed point representation
 * for world position.
 */
typedef int32_t qw;
#define QW_Q  12
#define QW_K  (1 << (QW_Q - 1))

/*
 * Angles are stored in a Q4.12 fixed point representation (16-bit), which is
 * designed to cover the range [-pi .. pi].
 */
typedef int16_t qa;
#define QA_Q  12
#define QA_K  (1 << (QA_Q - 1))

struct spos2
{
    int x;
    int y;
};

struct qwpos2
{
    qw x;
    qw y;
};

#define make_qw(v) (qw)((v) * (1 << QW_Q))
#define make_qwpos2(x, y) (struct qwpos2){ make_qw(x), make_qw(y) }
#define qw_to_int(q) ((int)((q) / (1 << QW_Q)))
#define qw_to_float(q) ((double)(q) / (1 << QW_Q))
#define qw_rebase(q, den) ((q) * den / (1 << QW_Q))

static inline qw qw_add(qw a, qw b)
{
    return a + b;
}

static inline qw qw_add_sat(qw a, qw b)
{
    qw result;
    int64_t tmp;

    tmp = (int64_t)a + (int64_t)b;
    if (tmp > 0x7FFFFF)
        tmp = 0x7FFFFF;
    if (tmp < -1 * 0x800000)
        tmp = -1 * 0x800000;
    result = (qw)tmp;

    return result;
}

static inline qw qw_sub(qw a, qw b)
{
    return a - b;
}

/* saturate to range of 24-bit signed int */
static inline qw qw_sat16(int64_t x)
{
    if (x > 0x7FFFFF) return 0x7FFFFF;
    else if (x < -0x800000) return -0x800000;
    else return (qw)x;
}

static inline qw qw_mul(qw a, qw b)
{
    qw result;
    int64_t temp;

    temp = (int64_t)a * (int64_t)b; /* result type is operand's type */
    /* Rounding; mid values are rounded up */
    temp += QW_K;
    /* Correct by dividing by base and saturate result */
    return qw_sat16(temp >> QW_Q);
}

static inline int qw_mul_to_int(qw a, int b)
{
    int64_t temp;

    temp = (int64_t)a * (int64_t)b; /* result type is operand's type */
    /* Rounding; mid values are rounded up */
    temp += QW_K;
    /* Correct by dividing by base */
    temp = (temp >> QW_Q);
    return qw_to_int(temp);
}

static inline qw qw_div(qw a, qw b)
{
    /* pre-multiply by the base (Upscale to Q32 so that the result will be in Q16 format) */
    int64_t temp = (int64_t)a << QW_Q;
    /* Rounding: mid values are rounded up (down for negative values). */
    /* OR compare most significant bits i.e. if (((temp >> 31) & 1) == ((b >> 15) & 1)) */
    if ((temp >= 0 && b >= 0) || (temp < 0 && b < 0)) {
        temp += b / 2;    /* OR shift 1 bit i.e. temp += (b >> 1); */
    }
    else {
        temp -= b / 2;    /* OR shift 1 bit i.e. temp -= (b >> 1); */
    }
    return (qw)(temp / b);
}

#define make_qa(v) (qa)((v) * (1 << QA_Q))
#define qa_to_int(q) ((int)((q) / (1 << QA_Q)))
#define qa_to_float(q) ((double)(q) / (1 << QA_Q))

static inline qa qa_add(qa a, qa b)
{
    return a + b;
}

static inline qa qa_add_sat(qa a, qa b)
{
    qa result;
    int32_t tmp;

    tmp = (int32_t)a + (int32_t)b;
    if (tmp > 0x7FFF)
        tmp = 0x7FFF;
    if (tmp < -1 * 0x8000)
        tmp = -1 * 0x8000;
    result = (qa)tmp;

    return result;
}

static inline qa qa_sub(qa a, qa b)
{
    return a - b;
}

/* saturate to range of 16-bit signed int */
static inline qa qa_sat16(int32_t x)
{
    if (x > 0x7FFF) return 0x7FFF;
    else if (x < -0x8000) return -0x8000;
    else return (qa)x;
}

static inline qa qa_mul(qa a, qa b)
{
    qa result;
    int32_t temp;

    temp = (int32_t)a * (int32_t)b; /* result type is operand's type */
    /* Rounding; mid values are rounded up */
    temp += QA_K;
    /* Correct by dividing by base and saturate result */
    result = qa_sat16(temp >> QA_Q);

    return result;
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
