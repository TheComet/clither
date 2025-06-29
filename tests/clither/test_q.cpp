#include "gmock/gmock.h"

extern "C" {
#include "clither/game/q.h"
}

#define NAME test_q

using namespace testing;

TEST(NAME, qw_to_q16_16)
{
    qw     a = 36550;
    q16_16 b = qw_to_q16_16(a);
    EXPECT_THAT(b, Eq(146200));
}

TEST(NAME, q16_16_to_qw)
{
    q16_16 a = 146200;
    q16_16 b = q16_16_to_qw(a);
    EXPECT_THAT(b, Eq(36550));
}

TEST(NAME, qa_to_pi)
{
    const double pi = 3.14159265358979323846;
    qa a = make_qa(pi / 2);
    EXPECT_THAT(qa_to_float(a), DoubleNear(pi / 2, 0.0005));
}
