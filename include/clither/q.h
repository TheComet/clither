#pragma once

#define _USE_MATH_DEFINES
#include <stdint.h>

typedef int32_t q16_16;
#define Q16_16_Q 16
#define Q16_16_K (1 << (Q16_16_Q - 1))

/*
 * Since the world size is fixed, we use a Q19.5 (24-bit) fixed point representation
 * for world position.
 */
typedef int32_t qw;
#define QW_Q    14
#define QW_K    (1 << (QW_Q - 1))
#define QW_MAX  ((1 << QW_Q) - 1)

/*
 * Angles are stored in a Q4.12 fixed point representation (16-bit), which is
 * designed to cover the range [-pi .. pi].
 */
typedef int16_t qa;
#define QA_Q  12
#define QA_K  (1 << (QA_Q - 1))
#define QA_PI 12867

struct spos
{
    int x;
    int y;
};

struct qwpos
{
    qw x;
    qw y;
};

struct qwaabb
{
    qw x1, y1;
    qw x2, y2;
};

#define make_q16_16(v) (qw)((v) * (1 << Q16_16_Q))
#define make_q16_16_2(v, div) (qw)((v) * (1 << Q16_16_Q) / (div))
#define q16_16_to_int(q) ((int)((q) / (1 << Q16_16_Q)))
#define q16_16_to_float(q) ((double)(q) / (1 << Q16_16_Q))
#define q16_16_rebase(q, den) ((q) * den / (1 << Q16_16_Q))
#define qw_to_q16_16(qw) ((int64_t)(qw) * (1 << Q16_16_Q) / (1 << QW_Q))

static inline q16_16 q16_16_add(q16_16 a, q16_16 b)
{
    return a + b;
}

static inline q16_16 q16_16_add_sat(q16_16 a, q16_16 b)
{
    int64_t tmp = (int64_t)a + (int64_t)b;
    if (tmp > 0x7FFFFFFF)
        tmp = 0x7FFFFFFF;
    if (tmp < -(int64_t)0x80000000L)
        tmp = -(int64_t)0x80000000L;
    return (q16_16)tmp;
}

static inline q16_16 q16_16_sub(q16_16 a, q16_16 b)
{
    return a - b;
}

/* saturate to range of 24-bit signed int */
static inline q16_16 q16_16_sat32(int64_t x)
{
    if (x > 0x7FFFFFFFL) return 0x7FFFFFFFL;
    else if (x < -(int64_t)0x80000000L) return -(int64_t)0x80000000L;
    else return (q16_16)x;
}

static inline q16_16 q16_16_mul(q16_16 a, q16_16 b)
{
    int64_t temp = (int64_t)a * (int64_t)b; /* result type is operand's type */
    /* Rounding; mid values are rounded up */
    temp += Q16_16_K;
    /* Correct by dividing by base and saturate result */
    return q16_16_sat32(temp >> Q16_16_Q);
}

static inline q16_16 q16_16_div(q16_16 a, q16_16 b)
{
    /* pre-multiply by the base (Upscale to Q64 so that the result will be in Q32 format) */
    int64_t temp = (int64_t)a << Q16_16_Q;
    /* Rounding: mid values are rounded up (down for negative values). */
    /* OR compare most significant bits i.e. if (((temp >> 31) & 1) == ((b >> 15) & 1)) */
    if ((temp >= 0 && b >= 0) || (temp < 0 && b < 0)) {
        temp += b / 2;    /* OR shift 1 bit i.e. temp += (b >> 1); */
    }
    else {
        temp -= b / 2;    /* OR shift 1 bit i.e. temp -= (b >> 1); */
    }
    return (q16_16)(temp / b);
}

#define make_qw(v) (qw)((v) * (1 << QW_Q))
#define make_qw2(v, div) (qw)((v) * (1 << QW_Q) / (div))
#define qw_to_int(q) ((int)((q) / (1 << QW_Q)))
#define qw_to_float(q) ((double)(q) / (1 << QW_Q))
#define qw_rebase(q, den) ((q) * den / (1 << QW_Q))
#define qw_rescale(q, num, den) ((int64_t)(q) * (num) / (den))
#define q16_16_to_qw(q16) qw_rescale(q16, 1 << QW_Q, 1 << Q16_16_Q)

static inline struct qwaabb make_qwaabbi(int x1, int y1, int x2, int y2)
{
    struct qwaabb bb;
    bb.x1 = make_qw(x1); bb.y1 = make_qw(y1);
    bb.x2 = make_qw(x2); bb.y2 = make_qw(y2);
    return bb;
}

static inline struct qwaabb make_qwaabbqw(qw x1, qw y1, qw x2, qw y2)
{
    struct qwaabb bb;
    bb.x1 = x1; bb.y1 = y1;
    bb.x2 = x2; bb.y2 = y2;
    return bb;
}

static inline qw qw_add(qw a, qw b)
{
    return a + b;
}

static inline qw qw_add_sat(qw a, qw b)
{
    int64_t tmp;

    tmp = (int64_t)a + (int64_t)b;
    if (tmp > 0x7FFFFF)
        tmp = 0x7FFFFF;
    if (tmp < -1 * 0x800000)
        tmp = -1 * 0x800000;
    return (qw)tmp;
}

static inline qw qw_sub(qw a, qw b)
{
    return a - b;
}

/* saturate to range of 24-bit signed int */
static inline qw qw_sat24(int64_t x)
{
    if (x > 0x7FFFFF) return 0x7FFFFF;
    else if (x < -0x800000) return -0x800000;
    else return (qw)x;
}

static inline qw qw_mul(qw a, qw b)
{
    int64_t temp = (int64_t)a * (int64_t)b; /* result type is operand's type */
    /* Rounding; mid values are rounded up */
    temp += QW_K;
    /* Correct by dividing by base and saturate result */
    return qw_sat24(temp >> QW_Q);
}

static inline int qw_mul_to_int(qw a, int b)
{
    int64_t temp = (int64_t)a * (int64_t)b; /* result type is operand's type */
    /* Rounding; mid values are rounded up */
    temp += QW_K;
    /* Correct by dividing by base */
    temp = (temp >> QW_Q);
    return qw_to_int(temp);
}

static inline qw qw_div(qw a, qw b)
{
    /* pre-multiply by the base (Upscale to Q64 so that the result will be in Q32 format) */
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

static inline qw qw_sq(qw q)
{
    return qw_mul(q, q);
}

#include <math.h>

static inline qw qw_sqrt(qw q)
{
    /* TODO */
    return make_qw(sqrt(qw_to_float(q)));
}

static inline struct qwpos make_qwposi(int x, int y)
{
    struct qwpos p;
    p.x = make_qw(x);
    p.y = make_qw(y);
    return p;
}
static inline struct qwpos make_qwposf(double x, double y)
{
    struct qwpos p;
    p.x = make_qw(x);
    p.y = make_qw(y);
    return p;
}
static inline struct qwpos make_qwposqw(qw x, qw y)
{
    struct qwpos p;
    p.x = x;
    p.y = y;
    return p;
}

static inline struct qwpos qwpos_normalize(struct qwpos p)
{
    qw len = qw_sqrt(qw_add(qw_mul(p.x, p.x), qw_mul(p.y, p.y)));
    if (len == 0)
        return make_qwposi(0, 0);
    p.x = qw_div(p.x, len);
    p.y = qw_div(p.y, len);
    return p;
}

#define make_qa(v) (qa)((v) * (1 << QA_Q))
#define make_qa2(v, div) (qa)((v) * (1 << QA_Q) / (div))
#define qa_to_int(q) ((int)((q) / (1 << QA_Q)))
#define qa_to_float(q) ((double)(q) / (1 << QA_Q))
#define qa_rescale(q, num, den) ((int64_t)(q) * (num) / (den))

/*! \brief Converts an 8-bit angle into fixed-point radians */
#define u8_to_qa(u) \
    (qa_rescale(2*QA_PI, u, 256) - QA_PI)

#define qa_to_u8(q) \
    qa_rescale((q + QA_PI), 256, 2*QA_PI)

static inline qa qa_wrapvalue(int32_t value)
{
    /* Ensure the angle remains in the range [-pi .. pi) or bad things happen */
    if (value < -QA_PI)
        value += 2 * QA_PI;
    if (value >= QA_PI)
        value -= 2 * QA_PI;
    return (qa)value;
}

static inline qa qa_add(qa a, qa b)
{
    return qa_wrapvalue((int32_t)a + (int32_t)b);
}

static inline qa qa_sub(qa a, qa b)
{
    return qa_wrapvalue((int32_t)a - (int32_t)b);
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
    int32_t temp = (int32_t)a * (int32_t)b; /* result type is operand's type */
    /* Rounding; mid values are rounded up */
    temp += QA_K;
    /* Correct by dividing by base and saturate result */
    return qa_sat16(temp >> QA_Q);
}

static inline qw qa_cos(qa q)
{
    /* TODO */
    return make_qw(cos(qa_to_float(q)));
}

static inline qw qa_sin(qa q)
{
    /* TODO */
    return make_qw(sin(qa_to_float(q)));
}
