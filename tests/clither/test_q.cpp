#include "gmock/gmock.h"
#include "clither/q.h"

#define NAME q

using namespace testing;

TEST(NAME, qw_to_q16_16)
{
    qw a = 36550;
    q16_16 b = qw_to_q16_16(a);
    EXPECT_THAT(b, Eq(146200));
}

TEST(NAME, q16_16_to_qw)
{
    q16_16 a = 146200;
    q16_16 b = q16_16_to_qw(a);
    EXPECT_THAT(b, Eq(36550));
}
