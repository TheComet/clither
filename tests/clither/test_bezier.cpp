#include "gmock/gmock.h"
#include "clither/bezier.h"
#include "clither/q.h"
#include "cstructures/vector.h"

#define NAME bezier

using namespace testing;

static const struct qwpos points1[] = {
    { make_qw2(-3018, 1<<14), make_qw2(-399, 1<<14) },
    { make_qw2(-3036, 1<<14), make_qw2(-381, 1<<14) },
    { make_qw2(-3053, 1<<14), make_qw2(-362, 1<<14) },
    { make_qw2(-3068, 1<<14), make_qw2(-342, 1<<14) },
    { make_qw2(-3082, 1<<14), make_qw2(-321, 1<<14) },
    { make_qw2(-3095, 1<<14), make_qw2(-299, 1<<14) },
    { make_qw2(-3106, 1<<14), make_qw2(-277, 1<<14) },
    { make_qw2(-3115, 1<<14), make_qw2(-255, 1<<14) },
    { make_qw2(-3123, 1<<14), make_qw2(-232, 1<<14) },
    { make_qw2(-3129, 1<<14), make_qw2(-209, 1<<14) },
    { make_qw2(-3134, 1<<14), make_qw2(-187, 1<<14) },
    { make_qw2(-3137, 1<<14), make_qw2(-165, 1<<14) },
    { make_qw2(-3139, 1<<14), make_qw2(-144, 1<<14) },
    { make_qw2(-3139, 1<<14), make_qw2(-124, 1<<14) },
    { make_qw2(-3139, 1<<14), make_qw2(-105, 1<<14) },
    { make_qw2(-3138, 1<<14), make_qw2(-87, 1<<14) },
    { make_qw2(-3136, 1<<14), make_qw2(-70, 1<<14) },
    { make_qw2(-3133, 1<<14), make_qw2(-54, 1<<14) },
    { make_qw2(-3129, 1<<14), make_qw2(-39, 1<<14) },
    { make_qw2(-3124, 1<<14), make_qw2(-25, 1<<14) },
    { make_qw2(-3119, 1<<14), make_qw2(-12, 1<<14) },
    { make_qw2(-3113, 1<<14), make_qw2(0, 1<<14) },
    { make_qw2(-3106, 1<<14), make_qw2(12, 1<<14) },
    { make_qw2(-3099, 1<<14), make_qw2(23, 1<<14) },
    { make_qw2(-3091, 1<<14), make_qw2(33, 1<<14) },
    { make_qw2(-3083, 1<<14), make_qw2(43, 1<<14) },
    { make_qw2(-3074, 1<<14), make_qw2(52, 1<<14) },
    { make_qw2(-3065, 1<<14), make_qw2(60, 1<<14) },
    { make_qw2(-3055, 1<<14), make_qw2(68, 1<<14) },
    { make_qw2(-3045, 1<<14), make_qw2(75, 1<<14) },
    { make_qw2(-3034, 1<<14), make_qw2(81, 1<<14) },
    { make_qw2(-3023, 1<<14), make_qw2(87, 1<<14) },
    { make_qw2(-3011, 1<<14), make_qw2(92, 1<<14) },
    { make_qw2(-2998, 1<<14), make_qw2(96, 1<<14) },
    { make_qw2(-2985, 1<<14), make_qw2(100, 1<<14) },
    { make_qw2(-2972, 1<<14), make_qw2(103, 1<<14) },
    { make_qw2(-2958, 1<<14), make_qw2(105, 1<<14) },
    { make_qw2(-2944, 1<<14), make_qw2(106, 1<<14) },
    { make_qw2(-2929, 1<<14), make_qw2(106, 1<<14) },
    { make_qw2(-2914, 1<<14), make_qw2(106, 1<<14) },
    { make_qw2(-2899, 1<<14), make_qw2(105, 1<<14) },
    { make_qw2(-2883, 1<<14), make_qw2(103, 1<<14) },
    { make_qw2(-2867, 1<<14), make_qw2(100, 1<<14) },
    { make_qw2(-2852, 1<<14), make_qw2(96, 1<<14) },
    { make_qw2(-2837, 1<<14), make_qw2(91, 1<<14) },
    { make_qw2(-2822, 1<<14), make_qw2(85, 1<<14) },
    { make_qw2(-2808, 1<<14), make_qw2(78, 1<<14) },
    { make_qw2(-2794, 1<<14), make_qw2(70, 1<<14) },
    { make_qw2(-2781, 1<<14), make_qw2(61, 1<<14) },
    { make_qw2(-2768, 1<<14), make_qw2(52, 1<<14) },
};

static const struct qwpos points2[] = {
    { make_qw2(31900, 1<<14), make_qw2(22377, 1<<14) },
    { make_qw2(31992, 1<<14), make_qw2(22452, 1<<14) },
    { make_qw2(32082, 1<<14), make_qw2(22529, 1<<14) },
    { make_qw2(32169, 1<<14), make_qw2(22608, 1<<14) },
    { make_qw2(32256, 1<<14), make_qw2(22686, 1<<14) },
    { make_qw2(32343, 1<<14), make_qw2(22764, 1<<14) },
    { make_qw2(32427, 1<<14), make_qw2(22844, 1<<14) },
    { make_qw2(32511, 1<<14), make_qw2(22924, 1<<14) },
    { make_qw2(32592, 1<<14), make_qw2(23005, 1<<14) },
    { make_qw2(32668, 1<<14), make_qw2(23089, 1<<14) },
    { make_qw2(32741, 1<<14), make_qw2(23174, 1<<14) },
    { make_qw2(32813, 1<<14), make_qw2(23258, 1<<14) },
    { make_qw2(32881, 1<<14), make_qw2(23345, 1<<14) },
    { make_qw2(32946, 1<<14), make_qw2(23433, 1<<14) },
};

static const struct qwpos points3[] = {
    { make_qw2(36550, 1<<14), make_qw2(25912, 1<<14) },
    { make_qw2(36503, 1<<14), make_qw2(26013, 1<<14) },
    { make_qw2(36453, 1<<14), make_qw2(26113, 1<<14) },
    { make_qw2(36401, 1<<14), make_qw2(26211, 1<<14) },
    { make_qw2(36346, 1<<14), make_qw2(26308, 1<<14) },
    { make_qw2(36291, 1<<14), make_qw2(26405, 1<<14) },
    { make_qw2(36234, 1<<14), make_qw2(26501, 1<<14) },
    { make_qw2(36174, 1<<14), make_qw2(26596, 1<<14) },
    { make_qw2(36112, 1<<14), make_qw2(26689, 1<<14) },
    { make_qw2(36047, 1<<14), make_qw2(26781, 1<<14) },
    { make_qw2(35982, 1<<14), make_qw2(26873, 1<<14) },
    { make_qw2(35915, 1<<14), make_qw2(26964, 1<<14) },
    { make_qw2(35845, 1<<14), make_qw2(27054, 1<<14) },
    { make_qw2(35773, 1<<14), make_qw2(27142, 1<<14) },
    { make_qw2(35701, 1<<14), make_qw2(27230, 1<<14) },
    { make_qw2(35627, 1<<14), make_qw2(27316, 1<<14) },
    { make_qw2(35552, 1<<14), make_qw2(27403, 1<<14) },
    { make_qw2(35475, 1<<14), make_qw2(27488, 1<<14) },
    { make_qw2(35396, 1<<14), make_qw2(27571, 1<<14) },
    { make_qw2(35317, 1<<14), make_qw2(27654, 1<<14) },
    { make_qw2(35236, 1<<14), make_qw2(27735, 1<<14) },
    { make_qw2(35153, 1<<14), make_qw2(27814, 1<<14) },
    { make_qw2(35069, 1<<14), make_qw2(27894, 1<<14) },
    { make_qw2(34983, 1<<14), make_qw2(27972, 1<<14) },
    { make_qw2(34895, 1<<14), make_qw2(28048, 1<<14) },
    { make_qw2(34807, 1<<14), make_qw2(28124, 1<<14) },
    { make_qw2(34717, 1<<14), make_qw2(28198, 1<<14) },
    { make_qw2(34626, 1<<14), make_qw2(28269, 1<<14) },
    { make_qw2(34533, 1<<14), make_qw2(28338, 1<<14) },
    { make_qw2(34438, 1<<14), make_qw2(28405, 1<<14) },
    { make_qw2(34342, 1<<14), make_qw2(28469, 1<<14) },
    { make_qw2(34247, 1<<14), make_qw2(28533, 1<<14) },
    { make_qw2(34151, 1<<14), make_qw2(28594, 1<<14) },
    { make_qw2(34055, 1<<14), make_qw2(28654, 1<<14) },
    { make_qw2(33959, 1<<14), make_qw2(28712, 1<<14) },
    { make_qw2(33863, 1<<14), make_qw2(28766, 1<<14) },
    { make_qw2(33766, 1<<14), make_qw2(28818, 1<<14) },
    { make_qw2(33669, 1<<14), make_qw2(28867, 1<<14) },
    { make_qw2(33573, 1<<14), make_qw2(28915, 1<<14) },
    { make_qw2(33477, 1<<14), make_qw2(28963, 1<<14) },
    { make_qw2(33381, 1<<14), make_qw2(29008, 1<<14) },
    { make_qw2(33284, 1<<14), make_qw2(29051, 1<<14) },
    { make_qw2(33188, 1<<14), make_qw2(29093, 1<<14) },
    { make_qw2(33091, 1<<14), make_qw2(29133, 1<<14) },
    { make_qw2(32995, 1<<14), make_qw2(29173, 1<<14) },
    { make_qw2(32898, 1<<14), make_qw2(29210, 1<<14) },
    { make_qw2(32800, 1<<14), make_qw2(29245, 1<<14) },
    { make_qw2(32703, 1<<14), make_qw2(29280, 1<<14) },
    { make_qw2(32605, 1<<14), make_qw2(29312, 1<<14) },
};
#define array_len(x) (sizeof(x) / sizeof(*x))

class NAME : public Test
{
public:
    void SetUp() override
    {
        vector_init(&points, sizeof(struct qwpos));
    }

    void TearDown() override
    {
        vector_deinit(&points);
    }

    struct cs_vector points;
};

TEST_F(NAME, misfit)
{
    /* 
     * This data was collected from an edge case where the curve would
     * horribly misfit. The bug in the end was an issue with qw_to_q16_16().
     */

    /*
     * T = [3080192, 1540080; 1540080, 1016018];
     * T_inv = [5759, -8730; -8730, 17460];
     * mx = -15780; qx = -115944
     * my = 13600; qy = 103648
     * x = [-115944, -138189, 238695, -131723];
     * y = [103648, 152256, 115366, 117246];
     * Ax = [-115944, -66735, 1197387, -1146431];
     * Ay = [103648, 145824, -256494, 124268];
     * hx = 32605; hy = 29312; ha = -20; hlb = 161; hlf = 0;
     * tx = 36550; ty = 25912; ta = -4676; tlb = 17; tlf = 208;
     * px = [36550, 36503, 36453, 36401, 36346, 36291, 36234, 36174, 36112, 36047, 35982, 35915, 35845, 35773, 35701, 35627, 35552, 35475, 35396, 35317, 35236, 35153, 35069, 34983, 34895, 34807, 34717, 34626, 34533, 34438, 34342, 34247, 34151, 34055, 33959, 33863, 33766, 33669, 33573, 33477, 33381, 33284, 33188, 33091, 32995, 32898, 32800, 32703, 32605];
     * py = [25912, 26013, 26113, 26211, 26308, 26405, 26501, 26596, 26689, 26781, 26873, 26964, 27054, 27142, 27230, 27316, 27403, 27488, 27571, 27654, 27735, 27814, 27894, 27972, 28048, 28124, 28198, 28269, 28338, 28405, 28469, 28533, 28594, 28654, 28712, 28766, 28818, 28867, 28915, 28963, 29008, 29051, 29093, 29133, 29173, 29210, 29245, 29280, 29312];
     */
    struct bezier_handle head = {
        { 0, 0 },
        0,
        0, 0
    };
    struct bezier_handle tail = {
        { make_qw2(36550, 1<<14), make_qw2(25912, 1<<14) },
        make_qa2(-4676, 1<<12),
        17, 0
    };

    for (int i = 0; i != array_len(points3); ++i)
        vector_push(&points, &points3[i]);
    bezier_fit_head(&head, &tail, &points);

    EXPECT_THAT(head.pos.x, Eq(32605));
    EXPECT_THAT(head.pos.y, Eq(29312));
    EXPECT_THAT(head.angle, Eq(-1205));
    EXPECT_THAT(head.len_backwards, Eq(25));
    EXPECT_THAT(head.len_forwards, Eq(0));

    EXPECT_THAT(tail.pos.x, Eq(36550));
    EXPECT_THAT(tail.pos.y, Eq(25912));
    EXPECT_THAT(tail.angle, Eq(-4676));
    EXPECT_THAT(tail.len_backwards, Eq(17));
    EXPECT_THAT(tail.len_forwards, Eq(27));
}

TEST_F(NAME, calc_equidistant_points_on_single_curve)
{
    cs_vector handles;
    vector_init(&handles, sizeof(struct bezier_handle));
    bezier_handle* tail = (bezier_handle*)vector_emplace(&handles);
    bezier_handle* head = (bezier_handle*)vector_emplace(&handles);

    bezier_handle_init(head, make_qwposi(2, 3), make_qa(M_PI / 4 * 3));
    bezier_handle_init(tail, make_qwposi(3, 4), make_qa(M_PI / 7));
    head->len_backwards = 255;
    tail->len_forwards = 255;

    bezier_calc_equidistant_points(&points, &handles, make_qw(0.1), 5);

    ASSERT_THAT(vector_count(&points), Eq(5));
    EXPECT_THAT(((bezier_point*)vector_get_element(&points, 0))->pos.x, Eq(make_qw(2)));
    EXPECT_THAT(((bezier_point*)vector_get_element(&points, 0))->pos.y, Eq(make_qw(3)));

    vector_deinit(&handles);
}

TEST_F(NAME, calc_equidistant_points_on_single_curve_no_space)
{
    cs_vector handles;
    vector_init(&handles, sizeof(struct bezier_handle));
    bezier_handle* tail = (bezier_handle*)vector_emplace(&handles);
    bezier_handle* head = (bezier_handle*)vector_emplace(&handles);

    bezier_handle_init(head, make_qwposi(2, 3), make_qa(M_PI / 4 * 3));
    bezier_handle_init(tail, make_qwposi(3, 4), make_qa(M_PI / 7));
    head->len_backwards = 255;
    tail->len_forwards = 255;

    bezier_calc_equidistant_points(&points, &handles, make_qw(0.1), 50);

    ASSERT_THAT(vector_count(&points), Eq(21));
    EXPECT_THAT(((bezier_point*)vector_get_element(&points, 0))->pos.x, Eq(make_qw(2)));
    EXPECT_THAT(((bezier_point*)vector_get_element(&points, 0))->pos.y, Eq(make_qw(3)));

    vector_deinit(&handles);
}

TEST_F(NAME, calc_equidistant_points_on_two_curves)
{
    cs_vector handles;
    vector_init(&handles, sizeof(struct bezier_handle));
    bezier_handle* tail = (bezier_handle*)vector_emplace(&handles);
    bezier_handle* mid = (bezier_handle*)vector_emplace(&handles);
    bezier_handle* head = (bezier_handle*)vector_emplace(&handles);

    bezier_handle_init(head, make_qwposf(0, 2), 0);
    bezier_handle_init(mid, make_qwposf(0, 1.5), 0);
    bezier_handle_init(tail, make_qwposf(0, 1), 0);

    bezier_calc_equidistant_points(&points, &handles, make_qw(0.4), 5);

    ASSERT_THAT(vector_count(&points), Eq(3));
    EXPECT_THAT(((bezier_point*)vector_get_element(&points, 0))->pos.x, Eq(make_qw(0)));
    EXPECT_THAT(((bezier_point*)vector_get_element(&points, 0))->pos.y, Eq(make_qw(2)));
    EXPECT_THAT(((bezier_point*)vector_get_element(&points, 1))->pos.x, Eq(0));
    EXPECT_THAT(((bezier_point*)vector_get_element(&points, 1))->pos.y, Eq(26215));
    EXPECT_THAT(((bezier_point*)vector_get_element(&points, 2))->pos.x, Eq(0));
    EXPECT_THAT(((bezier_point*)vector_get_element(&points, 2))->pos.y, Eq(19661));

    vector_deinit(&handles);
}

TEST_F(NAME, calc_equidistant_points_on_two_curves_spacing_larger_than_curve)
{
    cs_vector handles;
    vector_init(&handles, sizeof(struct bezier_handle));
    bezier_handle* tail = (bezier_handle*)vector_emplace(&handles);
    bezier_handle* mid = (bezier_handle*)vector_emplace(&handles);
    bezier_handle* head = (bezier_handle*)vector_emplace(&handles);

    bezier_handle_init(head, make_qwposf(0, 2), 0);
    bezier_handle_init(mid, make_qwposf(0, 1.5), 0);
    bezier_handle_init(tail, make_qwposf(0, 1), 0);

    bezier_calc_equidistant_points(&points, &handles, make_qw(0.8), 2);

    ASSERT_THAT(vector_count(&points), Eq(3));
    EXPECT_THAT(((bezier_point*)vector_get_element(&points, 0))->pos.x, Eq(make_qw(0)));
    EXPECT_THAT(((bezier_point*)vector_get_element(&points, 0))->pos.y, Eq(make_qw(2)));
    EXPECT_THAT(((bezier_point*)vector_get_element(&points, 1))->pos.x, Eq(0));
    EXPECT_THAT(((bezier_point*)vector_get_element(&points, 1))->pos.y, Eq(19661));

    vector_deinit(&handles);
}
