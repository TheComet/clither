#include "clither/util/morton.h"
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlong-long"
#endif

static uint64_t distribute64(uint32_t in)
{
    uint64_t x = in;
    /* clang-format off */
    x = (x | (x << 16)) & UINT64_C(0x0000FFFF0000FFFF);
    x = (x | (x << 8))  & UINT64_C(0x00FF00FF00FF00FF);
    x = (x | (x << 4))  & UINT64_C(0x0F0F0F0F0F0F0F0F);
    x = (x | (x << 2))  & UINT64_C(0x3333333333333333);
    x = (x | (x << 1))  & UINT64_C(0x5555555555555555);
    /* clang-format on */
    return x;
}

static uint32_t combine64(uint64_t x)
{
    /* clang-format off */
    x = (x | (x >> 0))  & UINT64_C(0x5555555555555555);
    x = (x | (x >> 1))  & UINT64_C(0x3333333333333333);
    x = (x | (x >> 2))  & UINT64_C(0x0F0F0F0F0F0F0F0F);
    x = (x | (x >> 4))  & UINT64_C(0x00FF00FF00FF00FF);
    x = (x | (x >> 8))  & UINT64_C(0x0000FFFF0000FFFF);
    x = (x | (x >> 16)) & UINT64_C(0x00000000FFFFFFFF);
    /* clang-format on */
    return (uint32_t)x;
}

static uint64_t distribute48s(int32_t in)
{
    uint64_t x = (in & 0xFFFFFF) ^ 0x800000;
    /* clang-format off */
    x = (x | (x << 16)) & UINT64_C(0x0000FFFF0000FFFF);
    x = (x | (x << 8))  & UINT64_C(0x00FF00FF00FF00FF);
    x = (x | (x << 4))  & UINT64_C(0x0F0F0F0F0F0F0F0F);
    x = (x | (x << 2))  & UINT64_C(0x3333333333333333);
    x = (x | (x << 1))  & UINT64_C(0x5555555555555555);
    /* clang-format on */
    return x;
}

static int32_t combine48s(uint64_t x)
{
    /* clang-format off */
    x = (x | (x >> 0))  & UINT64_C(0x5555555555555555);
    x = (x | (x >> 1))  & UINT64_C(0x3333333333333333);
    x = (x | (x >> 2))  & UINT64_C(0x0F0F0F0F0F0F0F0F);
    x = (x | (x >> 4))  & UINT64_C(0x00FF00FF00FF00FF);
    x = (x | (x >> 8))  & UINT64_C(0x0000FFFF0000FFFF);
    x = (x | (x >> 16)) & UINT64_C(0x00000000FFFFFFFF);
    /* clang-format on */
    x ^= 0x800000;
    /* 24-bit sign extension to 32-bit */
    if (x & 0x800000)
        x |= (0xFFU << 24);
    return (int32_t)x;
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

morton morton_encode_qwpos_generic(struct qwpos p)
{
    return distribute48s(p.x) | (distribute48s(p.y) << 1);
}

struct qwpos morton_decode_qwpos_generic(morton m)
{
    int32_t x = combine48s(m);
    int32_t y = combine48s(m >> 1);
    return make_qwposqw(x, y);
}
