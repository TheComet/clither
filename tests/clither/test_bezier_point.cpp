#include "gmock/gmock.h"

extern "C" {
#include "clither/bezier.h"
#include "clither/bezier_handle_rb.h"
#include "clither/bezier_point_vec.h"
#include "clither/q.h"
}

#define NAME bezier_point

using namespace testing;

namespace {
class NAME : public Test
{
public:
    void SetUp() override { bezier_point_vec_init(&points); }
    void TearDown() override { bezier_point_vec_deinit(points); }
    struct bezier_point_vec* points;
};

TEST_F(NAME, calc_equidistant_points_on_single_curve)
{
    bezier_handle_rb* handles;
    bezier_handle_rb_init(&handles);
    bezier_handle* tail = bezier_handle_rb_emplace_realloc(&handles);
    bezier_handle_init(tail, make_qwposi(3, 4), make_qa(M_PI / 7));
    tail->len_forwards = 255;

    bezier_handle* head = bezier_handle_rb_emplace_realloc(&handles);
    bezier_handle_init(head, make_qwposi(2, 3), make_qa(M_PI / 4 * 3));
    head->len_backwards = 255;

    bezier_calc_equidistant_points(
        &points, handles, make_qw(0.1), make_qw(0.4));

    ASSERT_THAT(vec_count(points), Eq(5));
    EXPECT_THAT(vec_get(points, 0)->pos.x, Eq(make_qw(2)));
    EXPECT_THAT(vec_get(points, 0)->pos.y, Eq(make_qw(3)));

    bezier_handle_rb_deinit(handles);
}

TEST_F(NAME, calc_equidistant_points_on_single_curve_no_space)
{
    bezier_handle_rb* handles;
    bezier_handle_rb_init(&handles);
    bezier_handle* tail = bezier_handle_rb_emplace_realloc(&handles);
    bezier_handle_init(tail, make_qwposi(3, 4), make_qa(M_PI / 7));
    tail->len_forwards = 255;

    bezier_handle* head = bezier_handle_rb_emplace_realloc(&handles);
    bezier_handle_init(head, make_qwposi(2, 3), make_qa(M_PI / 4 * 3));
    head->len_backwards = 255;

    bezier_calc_equidistant_points(&points, handles, make_qw(0.1), make_qw(5));

    ASSERT_THAT(vec_count(points), Eq(20));
    EXPECT_THAT(vec_get(points, 0)->pos.x, Eq(make_qw(2)));
    EXPECT_THAT(vec_get(points, 0)->pos.y, Eq(make_qw(3)));

    bezier_handle_rb_deinit(handles);
}

TEST_F(NAME, calc_equidistant_points_on_two_curves)
{
    bezier_handle_rb* handles;
    bezier_handle_rb_init(&handles);
    bezier_handle* tail = bezier_handle_rb_emplace_realloc(&handles);
    bezier_handle_init(tail, make_qwposf(0, 1), 0);

    bezier_handle* mid = bezier_handle_rb_emplace_realloc(&handles);
    bezier_handle_init(mid, make_qwposf(0, 1.5), 0);

    bezier_handle* head = bezier_handle_rb_emplace_realloc(&handles);
    bezier_handle_init(head, make_qwposf(0, 2), 0);

    bezier_calc_equidistant_points(&points, handles, make_qw(0.4), make_qw(1));

    ASSERT_THAT(vec_count(points), Eq(3));
    EXPECT_THAT(vec_get(points, 0)->pos.x, Eq(make_qw(0)));
    EXPECT_THAT(vec_get(points, 0)->pos.y, Eq(make_qw(2)));
    EXPECT_THAT(vec_get(points, 1)->pos.x, Eq(0));
    EXPECT_THAT(vec_get(points, 1)->pos.y, Eq(26215));
    EXPECT_THAT(vec_get(points, 2)->pos.x, Eq(0));
    EXPECT_THAT(vec_get(points, 2)->pos.y, Eq(19661));

    bezier_handle_rb_deinit(handles);
}

TEST_F(NAME, calc_equidistant_points_on_two_curves_spacing_larger_than_curve)
{
    bezier_handle_rb* handles;
    bezier_handle_rb_init(&handles);
    bezier_handle* tail = bezier_handle_rb_emplace_realloc(&handles);
    bezier_handle_init(tail, make_qwposf(0, 1), 0);

    bezier_handle* mid = bezier_handle_rb_emplace_realloc(&handles);
    bezier_handle_init(mid, make_qwposf(0, 1.5), 0);

    bezier_handle* head = bezier_handle_rb_emplace_realloc(&handles);
    bezier_handle_init(head, make_qwposf(0, 2), 0);

    bezier_calc_equidistant_points(&points, handles, make_qw(0.8), 3);

    ASSERT_THAT(vec_count(points), Eq(2));
    EXPECT_THAT(vec_get(points, 0)->pos.x, Eq(make_qw(0)));
    EXPECT_THAT(vec_get(points, 0)->pos.y, Eq(make_qw(2)));
    EXPECT_THAT(vec_get(points, 1)->pos.x, Eq(0));
    EXPECT_THAT(vec_get(points, 1)->pos.y, Eq(19661));

    bezier_handle_rb_deinit(handles);
}
} // namespace
