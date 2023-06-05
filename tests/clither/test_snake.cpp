#include "gmock/gmock.h"
#include "clither/controls.h"
#include "clither/snake.h"

#define NAME snake

using namespace testing;

TEST(NAME, roll_back_over_frame_boundary)
{
    struct snake s;
    snake_init(&s, make_qwposi(2, 2), "test");
    struct snake_head auth;
    snake_head_init(&auth, make_qwposi(2, 2));

    struct controls c;
    controls_init(&c);

    uint16_t frame_number = 65530;
    for (int i = 0; i < 10; ++i)
    {
        c.angle += 1;
        controls_add(&s.controls_buffer, &c, frame_number);
        snake_step(&s.data, &s.head, &c, 60);

        /* Step authoritative state up to 2 frames before a new segment is created */
        if (i < 5)
            snake_step_head(&auth, &c, 60);
        if (i == 4)
            snake_step_head(&auth, &c, 60); /* mispredict */

        frame_number++;
    }

    ASSERT_THAT(vector_count(&s.data.points), Eq(2));
    struct cs_vector* points0 = (cs_vector*)vector_get_element(&s.data.points, 0);
    struct cs_vector* points1 = (cs_vector*)vector_get_element(&s.data.points, 1);
    EXPECT_THAT(vector_count(points0), Eq(9));
    EXPECT_THAT(vector_count(points1), Eq(3));
    EXPECT_THAT(vector_count(&s.data.bezier_handles), Eq(3));

    /* Make sure auth state agrees */
    EXPECT_THAT(((qwpos*)vector_get_element(points0, 5))->x, Eq(auth.pos.x));
    EXPECT_THAT(((qwpos*)vector_get_element(points0, 5))->y, Eq(auth.pos.y));

    EXPECT_THAT(((qwpos*)vector_get_element(points0, 6))->x, Ne(auth.pos.x));
    EXPECT_THAT(((qwpos*)vector_get_element(points0, 6))->y, Ne(auth.pos.y));

    snake_deinit(&s);
}
