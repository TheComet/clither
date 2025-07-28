#include "gmock/gmock.h"

extern "C" {
#include "clither/game/bezier.h"
#include "clither/game/bezier_knot_rb.h"
#include "clither/game/bezier_segment_rb.h"
#include "clither/game/q.h"
#include "clither/game/qwpos_vec.h"
}

#define NAME test_bezier_sample

using namespace testing;

namespace {

class NAME : public Test
{
public:
    void SetUp() override
    {
        bezier_knot_rb_init(&knots);
        bezier_segment_rb_init(&segments);
    }

    void TearDown() override
    {
        bezier_knot_rb_deinit(knots);
        bezier_segment_rb_deinit(segments);
    }

    bezier_knot_rb*    knots;
    bezier_segment_rb* segments;
    std::vector<qwpos> points;
};

} // namespace

TEST_F(NAME, calc_equidistant_points_on_single_curve)
{
    bezier_knot* tail = bezier_knot_rb_emplace_realloc(&knots);
    bezier_knot_init(tail, make_qwposi(3, 4), make_qa(M_PI / 7), 0, 0);
    tail->len_forwards = 255;

    bezier_knot* head = bezier_knot_rb_emplace_realloc(&knots);
    bezier_knot_init(head, make_qwposi(2, 3), make_qa(M_PI / 4 * 3), 0, 0);
    head->len_backwards = 255;

    bezier_segment* segment = bezier_segment_rb_emplace_realloc(&segments);
    bezier_calc_segment(segment, head, tail);

    bezier_sample sample;
    for (bezier_sample_begin(
             &sample, segments, make_qw2(1, 10), make_qw2(5, 10));
         !bezier_sample_end(&sample);
         bezier_sample_next(&sample))
    {
        points.push_back(sample.pos);
    }

    ASSERT_THAT(points.size(), Eq(5));
    EXPECT_THAT(points[0].x, Eq(make_qw(2)));
    EXPECT_THAT(points[0].y, Eq(make_qw(3)));
}

TEST_F(NAME, calc_equidistant_points_on_single_curve_no_space)
{
    bezier_knot* tail = bezier_knot_rb_emplace_realloc(&knots);
    bezier_knot_init(tail, make_qwposi(3, 4), make_qa(M_PI / 7), 0, 0);
    tail->len_forwards = 255;

    bezier_knot* head = bezier_knot_rb_emplace_realloc(&knots);
    bezier_knot_init(head, make_qwposi(2, 3), make_qa(M_PI / 4 * 3), 0, 0);
    head->len_backwards = 255;

    bezier_segment* segment = bezier_segment_rb_emplace_realloc(&segments);
    bezier_calc_segment(segment, head, tail);

    bezier_sample sample;
    for (bezier_sample_begin(&sample, segments, make_qw2(1, 10), make_qw(5));
         !bezier_sample_end(&sample);
         bezier_sample_next(&sample))
    {
        points.push_back(sample.pos);
    }

    ASSERT_THAT(points.size(), Eq(33));
    EXPECT_THAT(points[0].x, Eq(make_qw(2)));
    EXPECT_THAT(points[0].y, Eq(make_qw(3)));
}

TEST_F(NAME, calc_equidistant_points_on_two_curves)
{
    bezier_knot* tail = bezier_knot_rb_emplace_realloc(&knots);
    bezier_knot_init(tail, make_qwposf(0, 1), 0, 0, 0);

    bezier_knot* mid = bezier_knot_rb_emplace_realloc(&knots);
    bezier_knot_init(mid, make_qwposf(0, 1.5), 0, 0, 0);

    bezier_knot* head = bezier_knot_rb_emplace_realloc(&knots);
    bezier_knot_init(head, make_qwposf(0, 2), 0, 0, 0);

    bezier_segment* segment1 = bezier_segment_rb_emplace_realloc(&segments);
    bezier_calc_segment(segment1, mid, tail);
    bezier_segment* segment2 = bezier_segment_rb_emplace_realloc(&segments);
    bezier_calc_segment(segment2, head, mid);

    bezier_sample sample;
    for (bezier_sample_begin(&sample, segments, make_qw2(2, 5), make_qw(5));
         !bezier_sample_end(&sample);
         bezier_sample_next(&sample))
    {
        points.push_back(sample.pos);
    }

    ASSERT_THAT(points.size(), Eq(3));
    EXPECT_THAT(points[0].x, Eq(make_qw(0)));
    EXPECT_THAT(points[0].y, Eq(make_qw(2)));
    EXPECT_THAT(points[1].x, Eq(0));
    EXPECT_THAT(points[1].y, Eq(make_qw(2 - 0.4 * 1) + 1));
    EXPECT_THAT(points[2].x, Eq(0));
    EXPECT_THAT(points[2].y, Eq(make_qw(2 - 0.4 * 2) + 3));
}

TEST_F(NAME, calc_equidistant_points_on_two_curves_spacing_larger_than_curve)
{
    bezier_knot* tail = bezier_knot_rb_emplace_realloc(&knots);
    bezier_knot_init(tail, make_qwposf(0, 1), 0, 0, 0);

    bezier_knot* mid = bezier_knot_rb_emplace_realloc(&knots);
    bezier_knot_init(mid, make_qwposf(0, 1.5), 0, 0, 0);

    bezier_knot* head = bezier_knot_rb_emplace_realloc(&knots);
    bezier_knot_init(head, make_qwposf(0, 2), 0, 0, 0);

    bezier_segment* segment1 = bezier_segment_rb_emplace_realloc(&segments);
    bezier_calc_segment(segment1, mid, tail);
    bezier_segment* segment2 = bezier_segment_rb_emplace_realloc(&segments);
    bezier_calc_segment(segment2, head, mid);

    bezier_sample sample;
    for (bezier_sample_begin(&sample, segments, make_qw2(4, 5), make_qw(1));
         !bezier_sample_end(&sample);
         bezier_sample_next(&sample))
    {
        points.push_back(sample.pos);
    }

    ASSERT_THAT(points.size(), Eq(2));
    EXPECT_THAT(points[0].x, Eq(make_qw(0)));
    EXPECT_THAT(points[0].y, Eq(make_qw(2)));
    EXPECT_THAT(points[1].x, Eq(0));
    EXPECT_THAT(points[1].y, Eq(make_qw2(6, 5)));
}
