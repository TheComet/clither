#include "gmock/gmock.h"
#include "clither/bezier.h"
#include "clither/q.h"
#include "cstructures/vector.h"

#define NAME bezier

using namespace testing;

static struct qwpos2 points1[50] = {
    { make_qw(-3018, 4096), make_qw(-399, 4096) },
    { make_qw(-3036, 4096), make_qw(-381, 4096) },
    { make_qw(-3053, 4096), make_qw(-362, 4096) },
    { make_qw(-3068, 4096), make_qw(-342, 4096) },
    { make_qw(-3082, 4096), make_qw(-321, 4096) },
    { make_qw(-3095, 4096), make_qw(-299, 4096) },
    { make_qw(-3106, 4096), make_qw(-277, 4096) },
    { make_qw(-3115, 4096), make_qw(-255, 4096) },
    { make_qw(-3123, 4096), make_qw(-232, 4096) },
    { make_qw(-3129, 4096), make_qw(-209, 4096) },
    { make_qw(-3134, 4096), make_qw(-187, 4096) },
    { make_qw(-3137, 4096), make_qw(-165, 4096) },
    { make_qw(-3139, 4096), make_qw(-144, 4096) },
    { make_qw(-3139, 4096), make_qw(-124, 4096) },
    { make_qw(-3139, 4096), make_qw(-105, 4096) },
    { make_qw(-3138, 4096), make_qw(-87, 4096) },
    { make_qw(-3136, 4096), make_qw(-70, 4096) },
    { make_qw(-3133, 4096), make_qw(-54, 4096) },
    { make_qw(-3129, 4096), make_qw(-39, 4096) },
    { make_qw(-3124, 4096), make_qw(-25, 4096) },
    { make_qw(-3119, 4096), make_qw(-12, 4096) },
    { make_qw(-3113, 4096), make_qw(0, 4096) },
    { make_qw(-3106, 4096), make_qw(12, 4096) },
    { make_qw(-3099, 4096), make_qw(23, 4096) },
    { make_qw(-3091, 4096), make_qw(33, 4096) },
    { make_qw(-3083, 4096), make_qw(43, 4096) },
    { make_qw(-3074, 4096), make_qw(52, 4096) },
    { make_qw(-3065, 4096), make_qw(60, 4096) },
    { make_qw(-3055, 4096), make_qw(68, 4096) },
    { make_qw(-3045, 4096), make_qw(75, 4096) },
    { make_qw(-3034, 4096), make_qw(81, 4096) },
    { make_qw(-3023, 4096), make_qw(87, 4096) },
    { make_qw(-3011, 4096), make_qw(92, 4096) },
    { make_qw(-2998, 4096), make_qw(96, 4096) },
    { make_qw(-2985, 4096), make_qw(100, 4096) },
    { make_qw(-2972, 4096), make_qw(103, 4096) },
    { make_qw(-2958, 4096), make_qw(105, 4096) },
    { make_qw(-2944, 4096), make_qw(106, 4096) },
    { make_qw(-2929, 4096), make_qw(106, 4096) },
    { make_qw(-2914, 4096), make_qw(106, 4096) },
    { make_qw(-2899, 4096), make_qw(105, 4096) },
    { make_qw(-2883, 4096), make_qw(103, 4096) },
    { make_qw(-2867, 4096), make_qw(100, 4096) },
    { make_qw(-2852, 4096), make_qw(96, 4096) },
    { make_qw(-2837, 4096), make_qw(91, 4096) },
    { make_qw(-2822, 4096), make_qw(85, 4096) },
    { make_qw(-2808, 4096), make_qw(78, 4096) },
    { make_qw(-2794, 4096), make_qw(70, 4096) },
    { make_qw(-2781, 4096), make_qw(61, 4096) },
    { make_qw(-2768, 4096), make_qw(52, 4096) },
};

class NAME : public Test
{
public:
    void SetUp() override
    {
        vector_init(&points, sizeof(struct qwpos2));
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
