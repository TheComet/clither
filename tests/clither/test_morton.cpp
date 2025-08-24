#include "gmock/gmock.h"

extern "C" {
#include "clither/util/morton.h"
#include "clither/config.h"
}

#define NAME test_morton

using namespace testing;

struct functions
{
    morton (*encode)(struct qwpos);
    struct qwpos (*decode)(morton);
};

struct NAME : TestWithParam<functions>
{
};

struct QwposEqMatcher : testing::MatcherInterface<struct qwpos>
{
    explicit QwposEqMatcher(struct qwpos expected) : expected(expected) {}
    explicit QwposEqMatcher(int32_t x, int32_t y) : expected(make_qwposqw(x, y))
    {
    }

    bool MatchAndExplain(
        struct qwpos pos, testing::MatchResultListener* listener) const override
    {
        *listener << "qwpos: [" << std::hex << pos.x << ", " << std::hex
                  << pos.y << "]";
        return expected.x == pos.x && expected.y == pos.y;
    }

    void DescribeTo(::std::ostream* os) const override
    {
        *os << "qwpos equals: [" << std::hex << expected.x << ", " << std::hex
            << expected.y << "]";
    }
    void DescribeNegationTo(::std::ostream* os) const override
    {
        *os << "qwpos does not equal: [" << std::hex << expected.x << ", "
            << std::hex << expected.y << "]";
    }

    struct qwpos expected;
};

inline testing::Matcher<struct qwpos> QwposEq(struct qwpos expected)
{
    return testing::MakeMatcher(new QwposEqMatcher(expected));
}
inline testing::Matcher<struct qwpos> QwposEq(int32_t x, int32_t y)
{
    return testing::MakeMatcher(new QwposEqMatcher(x, y));
}

TEST_P(NAME, from_qwpos_positive)
{
    // 00000011 -> 0x05
    // 00001100 -> 0x0C
    // 00110000 -> 0x30
    // 11000000 -> 0xC0
    EXPECT_THAT(GetParam().encode(make_qwposqw(1, 1)), Eq(0xC00000000003));
    EXPECT_THAT(GetParam().encode(make_qwposqw(2, 2)), Eq(0xC0000000000C));
    EXPECT_THAT(GetParam().encode(make_qwposqw(4, 4)), Eq(0xC00000000030));
    EXPECT_THAT(GetParam().encode(make_qwposqw(8, 8)), Eq(0xC000000000C0));
}

TEST_P(NAME, to_qwpos_positive)
{
    EXPECT_THAT(GetParam().decode(0xC00000000003), QwposEq(1, 1));
    EXPECT_THAT(GetParam().decode(0xC0000000000C), QwposEq(2, 2));
    EXPECT_THAT(GetParam().decode(0xC00000000030), QwposEq(4, 4));
    EXPECT_THAT(GetParam().decode(0xC000000000C0), QwposEq(8, 8));
}

TEST_P(NAME, from_qwpos_negative)
{
    // 11111111 -> 0xFF
    // 11111100 -> 0xFC
    // 11110000 -> 0xF0
    // 11000000 -> 0xC0
    morton m = GetParam().encode(make_qwposqw(-1, -1));
    (void)m;
    EXPECT_THAT(GetParam().encode(make_qwposqw(-1, -1)), Eq(0x3FFFFFFFFFFF));
    EXPECT_THAT(GetParam().encode(make_qwposqw(-2, -2)), Eq(0x3FFFFFFFFFFC));
    EXPECT_THAT(GetParam().encode(make_qwposqw(-4, -4)), Eq(0x3FFFFFFFFFF0));
    EXPECT_THAT(GetParam().encode(make_qwposqw(-8, -8)), Eq(0x3FFFFFFFFFC0));
}

TEST_P(NAME, to_qwpos_negative)
{
    EXPECT_THAT(GetParam().decode(0x3FFFFFFFFFFF), QwposEq(-1, -1));
    EXPECT_THAT(GetParam().decode(0x3FFFFFFFFFFC), QwposEq(-2, -2));
    EXPECT_THAT(GetParam().decode(0x3FFFFFFFFFF0), QwposEq(-4, -4));
    EXPECT_THAT(GetParam().decode(0x3FFFFFFFFFC0), QwposEq(-8, -8));
}

#if defined(CLITHER_ASM_OPTIMIZATIONS)
INSTANTIATE_TEST_CASE_P(
    generic,
    NAME,
    Values(functions{
        morton_encode_qwpos_generic, morton_decode_qwpos_generic}));

INSTANTIATE_TEST_CASE_P(
    asm,
    NAME,
    Values(functions{morton_encode_qwpos_asm, morton_decode_qwpos_asm}));
#else
INSTANTIATE_TEST_CASE_P(
    asm, NAME, Values(functions{morton_encode_qwpos, morton_decode_qwpos}));
#endif
