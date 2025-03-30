#include "gmock/gmock.h"

extern "C" {
#include "clither/util/morton.h"
}

#define NAME test_morton

using namespace testing;

struct NAME : Test
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

TEST_F(NAME, from_qwpos_positive)
{
    // 00000011 -> 0x05
    // 00001100 -> 0x0C
    // 00110000 -> 0x30
    // 11000000 -> 0xC0
    EXPECT_THAT(morton_encode_qwpos(make_qwposqw(1, 1)), Eq(0xC00000000003));
    EXPECT_THAT(morton_encode_qwpos(make_qwposqw(2, 2)), Eq(0xC0000000000C));
    EXPECT_THAT(morton_encode_qwpos(make_qwposqw(4, 4)), Eq(0xC00000000030));
    EXPECT_THAT(morton_encode_qwpos(make_qwposqw(8, 8)), Eq(0xC000000000C0));
}

TEST_F(NAME, to_qwpos_positive)
{
    EXPECT_THAT(morton_decode_qwpos(0xC00000000003), QwposEq(1, 1));
    EXPECT_THAT(morton_decode_qwpos(0xC0000000000C), QwposEq(2, 2));
    EXPECT_THAT(morton_decode_qwpos(0xC00000000030), QwposEq(4, 4));
    EXPECT_THAT(morton_decode_qwpos(0xC000000000C0), QwposEq(8, 8));
}

TEST_F(NAME, from_qwpos_negative)
{
    // 11111111 -> 0xFF
    // 11111100 -> 0xFC
    // 11110000 -> 0xF0
    // 11000000 -> 0xC0
    EXPECT_THAT(morton_encode_qwpos(make_qwposqw(-1, -1)), Eq(0x3FFFFFFFFFFF));
    EXPECT_THAT(morton_encode_qwpos(make_qwposqw(-2, -2)), Eq(0x3FFFFFFFFFFC));
    EXPECT_THAT(morton_encode_qwpos(make_qwposqw(-4, -4)), Eq(0x3FFFFFFFFFF0));
    EXPECT_THAT(morton_encode_qwpos(make_qwposqw(-8, -8)), Eq(0x3FFFFFFFFFC0));
}

TEST_F(NAME, to_qwpos_negative)
{
    EXPECT_THAT(morton_decode_qwpos(0x3FFFFFFFFFFF), QwposEq(-1, -1));
    EXPECT_THAT(morton_decode_qwpos(0x3FFFFFFFFFFC), QwposEq(-2, -2));
    EXPECT_THAT(morton_decode_qwpos(0x3FFFFFFFFFF0), QwposEq(-4, -4));
    EXPECT_THAT(morton_decode_qwpos(0x3FFFFFFFFFC0), QwposEq(-8, -8));
}
