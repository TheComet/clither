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

TEST_F(NAME, weird_edgecase)
{
    struct bezier_sample sample;
    bezier_segment*      seg;

    seg = bezier_segment_rb_emplace_realloc(&segments);
    seg->p[0] = make_qwposqw(-824318, -1469704);
    seg->p[1] = make_qwposqw(-825089, -1471816);
    seg->p[2] = make_qwposqw(-826996, -1472786);
    seg->p[3] = make_qwposqw(-828023, -1472786);
    seg->coeff[0] = make_qwposqw(-2313, -6336);
    seg->coeff[1] = make_qwposqw(-3408, 3426);
    seg->coeff[2] = make_qwposqw(2016, -172);
    seg->fallback_tangent = make_qwposqw(-5623, -15388);

    seg = bezier_segment_rb_emplace_realloc(&segments);
    seg->p[0] = make_qwposqw(-825777, -1463793);
    seg->p[1] = make_qwposqw(-824191, -1465199);
    seg->p[2] = make_qwposqw(-823591, -1467713);
    seg->p[3] = make_qwposqw(-824318, -1469704);
    seg->coeff[0] = make_qwposqw(4758, -4218);
    seg->coeff[1] = make_qwposqw(-2958, -3324);
    seg->coeff[2] = make_qwposqw(-341, 1631);
    seg->fallback_tangent = make_qwposqw(12262, -10866);

    seg = bezier_segment_rb_emplace_realloc(&segments);
    seg->p[0] = make_qwposqw(-828777, -1462249);
    seg->p[1] = make_qwposqw(-827759, -1462642);
    seg->p[2] = make_qwposqw(-826594, -1463069);
    seg->p[3] = make_qwposqw(-825777, -1463793);
    seg->coeff[0] = make_qwposqw(3054, -1179);
    seg->coeff[1] = make_qwposqw(441, -102);
    seg->coeff[2] = make_qwposqw(-495, -263);
    seg->fallback_tangent = make_qwposqw(15281, -5907);

    seg = bezier_segment_rb_emplace_realloc(&segments);
    seg->p[0] = make_qwposqw(-831764, -1461659);
    seg->p[1] = make_qwposqw(-830805, -1461750);
    seg->p[2] = make_qwposqw(-829675, -1461902);
    seg->p[3] = make_qwposqw(-828777, -1462249);
    seg->coeff[0] = make_qwposqw(2877, -273);
    seg->coeff[1] = make_qwposqw(513, -183);
    seg->coeff[2] = make_qwposqw(-403, -134);
    seg->fallback_tangent = make_qwposqw(16309, -1557);

    seg = bezier_segment_rb_emplace_realloc(&segments);
    seg->p[0] = make_qwposqw(-843320, -1461474);
    seg->p[1] = make_qwposqw(-839530, -1461504);
    seg->p[2] = make_qwposqw(-835601, -1461293);
    seg->p[3] = make_qwposqw(-831764, -1461659);
    seg->coeff[0] = make_qwposqw(11370, -90);
    seg->coeff[1] = make_qwposqw(417, 723);
    seg->coeff[2] = make_qwposqw(-231, -818);
    seg->fallback_tangent = make_qwposqw(16383, -131);

    seg = bezier_segment_rb_emplace_realloc(&segments);
    seg->p[0] = make_qwposqw(-864060, -1461474);
    seg->p[1] = make_qwposqw(-857186, -1461474);
    seg->p[2] = make_qwposqw(-850194, -1461420);
    seg->p[3] = make_qwposqw(-843320, -1461474);
    seg->coeff[0] = make_qwposqw(20622, 0);
    seg->coeff[1] = make_qwposqw(354, 162);
    seg->coeff[2] = make_qwposqw(-236, -162);
    seg->fallback_tangent = make_qwposqw(16384, 0);

    seg = bezier_segment_rb_emplace_realloc(&segments);
    seg->p[0] = make_qwposqw(-887520, -1461474);
    seg->p[1] = make_qwposqw(-879746, -1461474);
    seg->p[2] = make_qwposqw(-871834, -1461474);
    seg->p[3] = make_qwposqw(-864060, -1461474);
    seg->coeff[0] = make_qwposqw(23322, 0);
    seg->coeff[1] = make_qwposqw(414, 0);
    seg->coeff[2] = make_qwposqw(-276, 0);
    seg->fallback_tangent = make_qwposqw(16384, 0);

    seg = bezier_segment_rb_emplace_realloc(&segments);
    seg->p[0] = make_qwposqw(-897040, -1461474);
    seg->p[1] = make_qwposqw(-893892, -1461474);
    seg->p[2] = make_qwposqw(-890668, -1461474);
    seg->p[3] = make_qwposqw(-887520, -1461474);
    seg->coeff[0] = make_qwposqw(9444, 0);
    seg->coeff[1] = make_qwposqw(228, 0);
    seg->coeff[2] = make_qwposqw(-152, 0);
    seg->fallback_tangent = make_qwposqw(16384, 0);

    qwpos_vec* samples;
    qwpos_vec_init(&samples);
    for (bezier_sample_begin(
             &sample, segments, make_qw2(1, 6), make_qw2(490, 128));
         !bezier_sample_end(&sample);
         bezier_sample_next(&sample))
    {
        struct qwpos* pos = qwpos_vec_emplace(&samples);
        *pos = sample.pos;
    }
    int segments_left = bezier_sample_idx(&sample);

    EXPECT_THAT(vec_count(samples), Eq(23));
    EXPECT_THAT(segments_left, Eq(4));
    qwpos_vec_deinit(samples);
}
