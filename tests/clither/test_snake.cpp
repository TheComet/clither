#include "gmock/gmock.h"
#include "clither/controls.h"
#include "clither/snake.h"

#define NAME snake

using namespace testing;

TEST(NAME, roll_back_over_frame_boundary)
{
    struct snake s;
    snake_init(&s, make_qwposi(2, 2), "test");
    struct snake_head auth1, auth2;
    snake_head_init(&auth1, make_qwposi(2, 2));
    snake_head_init(&auth2, make_qwposi(2, 2));

    struct controls c;
    controls_init(&c);

    uint16_t frame_number = 65535 - 9;
    uint16_t mispredict_frame = frame_number;
    for (int i = 0; i < 12; ++i)
    {
        c.angle += 2;
        controls_rb_put(&s.controls_rb, &c, frame_number);
        snake_step(&s.data, &s.head, &c, 60);

        /* Step authoritative state up to 2 frames before a new segment is created */
        if (i < 8)
        {
            snake_step_head(&auth1, &c, 60);
            snake_step_head(&auth2, &c, 60);
            if (i == 7)
                snake_step_head(&auth2, &c, 60); /* mispredict */
            mispredict_frame++;
        }

        frame_number++;
    }

    ASSERT_THAT(vector_count(&s.data.points), Eq(2));
    struct cs_vector* points0 = (cs_vector*)vector_get_element(&s.data.points, 0);
    struct cs_vector* points1 = (cs_vector*)vector_get_element(&s.data.points, 1);
    EXPECT_THAT(vector_count(points0), Eq(11));
    EXPECT_THAT(vector_count(points1), Eq(3));
    EXPECT_THAT(vector_count(&s.data.bezier_handles), Eq(3));

    /* Make sure auth1 state agrees */
    EXPECT_THAT(((qwpos*)vector_get_element(points0, 8))->x, Eq(auth1.pos.x));
    EXPECT_THAT(((qwpos*)vector_get_element(points0, 8))->y, Eq(auth1.pos.y));

    /* Make sure auth2 state (the mispredicted state) disagrees */
    EXPECT_THAT(((qwpos*)vector_get_element(points0, 9))->x, Ne(auth2.pos.x));
    EXPECT_THAT(((qwpos*)vector_get_element(points0, 9))->y, Ne(auth2.pos.y));

    snake_ack_frame(&s.data, &s.head_ack, &s.head, &auth1, &s.controls_rb, mispredict_frame, 60);

    snake_deinit(&s);
}
