#include "gmock/gmock.h"
#include "clither/controls.h"
#include "clither/snake.h"
#include "clither/wrap.h"

#define NAME snake

using namespace testing;

TEST(NAME, roll_back_over_frame_boundary)
{
    struct snake client, server;
    snake_init(&client, make_qwposi(2, 2), "client");
    snake_init(&server, make_qwposi(2, 2), "server");

    struct controls c;
    controls_init(&c);

    uint16_t frame_number = 65535 - 10;
    uint16_t mispredict_frame = frame_number + 4;
    for (int i = 0; i < 14; ++i)
    {
        c.angle += 2;
        controls_rb_put(&client.controls_rb, &c, frame_number);
        snake_step(&client.data, &client.head, &c, 60);

        if (u16_le_wrap(frame_number, mispredict_frame))
        {
            snake_step(&server.data, &server.head, &c, 60);
            if (frame_number == mispredict_frame)
                snake_step(&server.data, &server.head, &c, 60);  /* mispredict */
        }

        frame_number++;
    }

    /* Make sure we have 2 bezier segments */
    ASSERT_THAT(vector_count(&client.data.points), Eq(2));
    struct cs_vector* client_pts0 = (cs_vector*)vector_get_element(&client.data.points, 0);
    struct cs_vector* client_pts1 = (cs_vector*)vector_get_element(&client.data.points, 1);
    EXPECT_THAT(vector_count(client_pts0), Eq(11));
    EXPECT_THAT(vector_count(client_pts1), Eq(5));
    EXPECT_THAT(vector_count(&client.data.bezier_handles), Eq(3));

    ASSERT_THAT(vector_count(&server.data.points), Eq(1));
    struct cs_vector* server_pts0 = (cs_vector*)vector_get_element(&server.data.points, 0);
    EXPECT_THAT(vector_count(server_pts0), Eq(7));
    EXPECT_THAT(vector_count(&server.data.bezier_handles), Eq(2));

    /* Make sure sim agrees up to mispredicted frame */
    EXPECT_THAT(
        ((qwpos*)vector_get_element(client_pts0, 5))->x, Eq(
        ((qwpos*)vector_get_element(server_pts0, 5))->x));
    EXPECT_THAT(
        ((qwpos*)vector_get_element(client_pts0, 5))->y, Eq(
        ((qwpos*)vector_get_element(server_pts0, 5))->y));
    EXPECT_THAT(
        ((qwpos*)vector_get_element(client_pts0, 6))->x, Ne(
            ((qwpos*)vector_get_element(server_pts0, 6))->x));
    EXPECT_THAT(
        ((qwpos*)vector_get_element(client_pts0, 6))->y, Ne(
            ((qwpos*)vector_get_element(server_pts0, 6))->y));

    /* Everything is set up so that "mispredict_frame" is the last frame on which
     * the simulation will match up. Going from mispredict_frame to mispredict_frame+1
     * will cause a roll back */
    snake_ack_frame(&client.data, &client.head_ack, &client.head, &server.head, &client.controls_rb, mispredict_frame, 60);

    snake_deinit(&client);
    snake_deinit(&server);
}