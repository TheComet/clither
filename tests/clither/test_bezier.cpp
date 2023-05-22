#include "gmock/gmock.h"
#include "clither/bezier.h"
#include "clither/q.h"
#include "cstructures/vector.h"

#define NAME bezier

using namespace testing;

static struct qwpos points1[50] = {
    { make_qw2(-3018, 4096), make_qw2(-399, 4096) },
    { make_qw2(-3036, 4096), make_qw2(-381, 4096) },
    { make_qw2(-3053, 4096), make_qw2(-362, 4096) },
    { make_qw2(-3068, 4096), make_qw2(-342, 4096) },
    { make_qw2(-3082, 4096), make_qw2(-321, 4096) },
    { make_qw2(-3095, 4096), make_qw2(-299, 4096) },
    { make_qw2(-3106, 4096), make_qw2(-277, 4096) },
    { make_qw2(-3115, 4096), make_qw2(-255, 4096) },
    { make_qw2(-3123, 4096), make_qw2(-232, 4096) },
    { make_qw2(-3129, 4096), make_qw2(-209, 4096) },
    { make_qw2(-3134, 4096), make_qw2(-187, 4096) },
    { make_qw2(-3137, 4096), make_qw2(-165, 4096) },
    { make_qw2(-3139, 4096), make_qw2(-144, 4096) },
    { make_qw2(-3139, 4096), make_qw2(-124, 4096) },
    { make_qw2(-3139, 4096), make_qw2(-105, 4096) },
    { make_qw2(-3138, 4096), make_qw2(-87, 4096) },
    { make_qw2(-3136, 4096), make_qw2(-70, 4096) },
    { make_qw2(-3133, 4096), make_qw2(-54, 4096) },
    { make_qw2(-3129, 4096), make_qw2(-39, 4096) },
    { make_qw2(-3124, 4096), make_qw2(-25, 4096) },
    { make_qw2(-3119, 4096), make_qw2(-12, 4096) },
    { make_qw2(-3113, 4096), make_qw2(0, 4096) },
    { make_qw2(-3106, 4096), make_qw2(12, 4096) },
    { make_qw2(-3099, 4096), make_qw2(23, 4096) },
    { make_qw2(-3091, 4096), make_qw2(33, 4096) },
    { make_qw2(-3083, 4096), make_qw2(43, 4096) },
    { make_qw2(-3074, 4096), make_qw2(52, 4096) },
    { make_qw2(-3065, 4096), make_qw2(60, 4096) },
    { make_qw2(-3055, 4096), make_qw2(68, 4096) },
    { make_qw2(-3045, 4096), make_qw2(75, 4096) },
    { make_qw2(-3034, 4096), make_qw2(81, 4096) },
    { make_qw2(-3023, 4096), make_qw2(87, 4096) },
    { make_qw2(-3011, 4096), make_qw2(92, 4096) },
    { make_qw2(-2998, 4096), make_qw2(96, 4096) },
    { make_qw2(-2985, 4096), make_qw2(100, 4096) },
    { make_qw2(-2972, 4096), make_qw2(103, 4096) },
    { make_qw2(-2958, 4096), make_qw2(105, 4096) },
    { make_qw2(-2944, 4096), make_qw2(106, 4096) },
    { make_qw2(-2929, 4096), make_qw2(106, 4096) },
    { make_qw2(-2914, 4096), make_qw2(106, 4096) },
    { make_qw2(-2899, 4096), make_qw2(105, 4096) },
    { make_qw2(-2883, 4096), make_qw2(103, 4096) },
    { make_qw2(-2867, 4096), make_qw2(100, 4096) },
    { make_qw2(-2852, 4096), make_qw2(96, 4096) },
    { make_qw2(-2837, 4096), make_qw2(91, 4096) },
    { make_qw2(-2822, 4096), make_qw2(85, 4096) },
    { make_qw2(-2808, 4096), make_qw2(78, 4096) },
    { make_qw2(-2794, 4096), make_qw2(70, 4096) },
    { make_qw2(-2781, 4096), make_qw2(61, 4096) },
    { make_qw2(-2768, 4096), make_qw2(52, 4096) },
};

class NAME : public Test
{
public:
    void SetUp() override
    {
        vector_init(&points, sizeof(struct qwpos));
        for (int i = 0; i != 50; ++i)
            vector_push(&points, &points1[i]);
    }

    void TearDown() override
    {
        vector_deinit(&points);
    }

    struct cs_vector points;
};

TEST_F(NAME, test)
{
    struct bezier_handle head, tail;
    bezier_handle_init(&head, points1[0]);
    bezier_handle_init(&tail, points1[49]);
    bezier_fit_head(&head, &tail, &points);
}
