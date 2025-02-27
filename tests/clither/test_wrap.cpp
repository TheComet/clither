#include "gmock/gmock.h"

extern "C" {
#include "clither/wrap.h"
}

#define NAME wrap

using namespace testing;

TEST(NAME, u8_gt_wrap)
{
    uint8_t a = 5;
    uint8_t b = 2;
    uint8_t c = 254;
    EXPECT_THAT(u8_gt_wrap(a, b), IsTrue());
    EXPECT_THAT(u8_gt_wrap(b, a), IsFalse());
    EXPECT_THAT(u8_gt_wrap(a, c), IsTrue());
    EXPECT_THAT(u8_gt_wrap(c, a), IsFalse());
    EXPECT_THAT(u8_gt_wrap(a, a), IsFalse());
    EXPECT_THAT(u8_gt_wrap(c, c), IsFalse());
}

TEST(NAME, u8_ge_wrap)
{
    uint8_t a = 5;
    uint8_t b = 2;
    uint8_t c = 254;
    EXPECT_THAT(u8_ge_wrap(a, b), IsTrue());
    EXPECT_THAT(u8_ge_wrap(b, a), IsFalse());
    EXPECT_THAT(u8_ge_wrap(a, c), IsTrue());
    EXPECT_THAT(u8_ge_wrap(c, a), IsFalse());
    EXPECT_THAT(u8_ge_wrap(a, a), IsTrue());
    EXPECT_THAT(u8_ge_wrap(c, c), IsTrue());
}

TEST(NAME, u16_gt_wrap)
{
    uint16_t a = 5;
    uint16_t b = 2;
    uint16_t c = 65532;
    EXPECT_THAT(u16_gt_wrap(a, b), IsTrue());
    EXPECT_THAT(u16_gt_wrap(b, a), IsFalse());
    EXPECT_THAT(u16_gt_wrap(a, c), IsTrue());
    EXPECT_THAT(u16_gt_wrap(c, a), IsFalse());
    EXPECT_THAT(u16_gt_wrap(a, a), IsFalse());
    EXPECT_THAT(u16_gt_wrap(c, c), IsFalse());
}

TEST(NAME, u16_ge_wrap)
{
    uint16_t a = 5;
    uint16_t b = 2;
    uint16_t c = 65532;
    EXPECT_THAT(u16_ge_wrap(a, b), IsTrue());
    EXPECT_THAT(u16_ge_wrap(b, a), IsFalse());
    EXPECT_THAT(u16_ge_wrap(a, c), IsTrue());
    EXPECT_THAT(u16_ge_wrap(c, a), IsFalse());
    EXPECT_THAT(u16_ge_wrap(a, a), IsTrue());
    EXPECT_THAT(u16_ge_wrap(c, c), IsTrue());
}

TEST(NAME, u16_sub_wrap)
{
    uint16_t a = 5;
    uint16_t b = 2;
    uint16_t c = 65532;
    EXPECT_THAT(u16_sub_wrap(a, b), Eq(3));
    EXPECT_THAT(u16_sub_wrap(b, a), Eq(-3));
    EXPECT_THAT(u16_sub_wrap(a, c), Eq(9));
    EXPECT_THAT(u16_sub_wrap(c, a), Eq(-9));
}
