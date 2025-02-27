#include "gmock/gmock.h"

extern "C" {
#include "clither/bezier.h"
#include "clither/q.h"
}

#define NAME bezier_aabb

using namespace testing;

class NAME : public Test
{
public:
};

TEST_F(NAME, aabb_straight_line)
{
    bezier_handle head, tail;
    bezier_handle_init(&head, make_qwposf(0.2, 0.3), 0);
    bezier_handle_init(&tail, make_qwposf(0.8, 0.7), 0);

    qwaabb bb;
    bezier_calc_aabb(&bb, &head, &tail);

    EXPECT_THAT(bb.x1, Eq(make_qw(0.2)));
    EXPECT_THAT(bb.x2, Eq(make_qw(0.8)));
    EXPECT_THAT(bb.y1, Eq(make_qw(0.3)));
    EXPECT_THAT(bb.y2, Eq(make_qw(0.7)));
}

TEST_F(NAME, aabb_x_extremities_1)
{
    bezier_handle head, tail;
    bezier_handle_init(&head, make_qwposf(0.2, 0.1), 0);
    bezier_handle_init(&tail, make_qwposf(0.2, 0.9), 0);
    head.len_backwards = 255;
    tail.len_forwards = 255;

    qwaabb bb;
    bezier_calc_aabb(&bb, &head, &tail);

    EXPECT_THAT(bb.x1, Eq(make_qw2(-1458, 1 << 14)));
    EXPECT_THAT(bb.x2, Eq(make_qw2(8004, 1 << 14)));
    EXPECT_THAT(bb.y1, Eq(make_qw(0.1)));
    EXPECT_THAT(bb.y2, Eq(make_qw(0.9)));
}

TEST_F(NAME, aabb_x_extremities_2)
{
    bezier_handle head, tail;
    bezier_handle_init(&head, make_qwposf(0.2, 0.1), 0);
    bezier_handle_init(&tail, make_qwposf(0.2, 0.9), 0);
    head.len_backwards = 0;
    tail.len_forwards = 255;

    qwaabb bb;
    bezier_calc_aabb(&bb, &head, &tail);

    EXPECT_THAT(bb.x1, Eq(make_qw2(-4005, 1 << 14)));
    EXPECT_THAT(bb.x2, Eq(make_qw(0.2)));
    EXPECT_THAT(bb.y1, Eq(make_qw(0.1)));
    EXPECT_THAT(bb.y2, Eq(make_qw(0.9)));
}

TEST_F(NAME, aabb_x_extremities_3)
{
    bezier_handle head, tail;
    bezier_handle_init(&head, make_qwposf(0.2, 0.1), 0);
    bezier_handle_init(&tail, make_qwposf(0.2, 0.9), 0);
    head.len_backwards = 255;
    tail.len_forwards = 0;

    qwaabb bb;
    bezier_calc_aabb(&bb, &head, &tail);

    EXPECT_THAT(bb.x1, Eq(make_qw(0.2)));
    EXPECT_THAT(bb.x2, Eq(make_qw2(10560, 1 << 14)));
    EXPECT_THAT(bb.y1, Eq(make_qw(0.1)));
    EXPECT_THAT(bb.y2, Eq(make_qw(0.9)));
}

TEST_F(NAME, aabb_y_extremities_1)
{
    bezier_handle head, tail;
    bezier_handle_init(&head, make_qwposf(0.1, 0.2), QA_PI / 2);
    bezier_handle_init(&tail, make_qwposf(0.9, 0.2), QA_PI / 2);
    head.len_backwards = 255;
    tail.len_forwards = 255;

    qwaabb bb;
    bezier_calc_aabb(&bb, &head, &tail);

    EXPECT_THAT(bb.x1, Eq(make_qw(0.1)));
    EXPECT_THAT(bb.x2, Eq(make_qw(0.9)));
    EXPECT_THAT(bb.y1, Eq(make_qw2(-1457, 1 << 14)));
    EXPECT_THAT(bb.y2, Eq(make_qw2(8003, 1 << 14)));
}

TEST_F(NAME, aabb_y_extremities_2)
{
    bezier_handle head, tail;
    bezier_handle_init(&head, make_qwposf(0.1, 0.2), QA_PI / 2);
    bezier_handle_init(&tail, make_qwposf(0.9, 0.2), QA_PI / 2);
    head.len_backwards = 0;
    tail.len_forwards = 255;

    qwaabb bb;
    bezier_calc_aabb(&bb, &head, &tail);

    EXPECT_THAT(bb.x1, Eq(make_qw(0.1)));
    EXPECT_THAT(bb.x2, Eq(make_qw(0.9)));
    EXPECT_THAT(bb.y1, Eq(make_qw2(-4005, 1 << 14)));
    EXPECT_THAT(bb.y2, Eq(make_qw(0.2)));
}

TEST_F(NAME, aabb_y_extremities_3)
{
    bezier_handle head, tail;
    bezier_handle_init(&head, make_qwposf(0.1, 0.2), QA_PI / 2);
    bezier_handle_init(&tail, make_qwposf(0.9, 0.2), QA_PI / 2);
    head.len_backwards = 255;
    tail.len_forwards = 0;

    qwaabb bb;
    bezier_calc_aabb(&bb, &head, &tail);

    EXPECT_THAT(bb.x1, Eq(make_qw(0.1)));
    EXPECT_THAT(bb.x2, Eq(make_qw(0.9)));
    EXPECT_THAT(bb.y1, Eq(make_qw(0.2)));
    EXPECT_THAT(bb.y2, Eq(make_qw2(10560, 1 << 14)));
}
