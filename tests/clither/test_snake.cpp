#include "gmock/gmock.h"
#include "clither/command.h"
#include "clither/log.h"
#include "clither/snake.h"
#include "clither/wrap.h"
#include "cstructures/rb.h"

#define NAME snake

using namespace testing;

static void print_points_lists(const struct cs_vector* points)
{
    int points_offset = -1;
    const struct cs_vector* points_ptr = points;
    log_raw("px = [");
    for (int i = 0; i < 199; ++i)
    {
        if (i - points_offset >= (int)vector_count(points_ptr) - 1)
        {
            points_offset = i;
            points_ptr++;
        }

        printf("%d ", i);
        qwpos* p = (qwpos*)vector_get(points_ptr, i - points_offset);

        if (i != 0)
            log_raw(", ");
        log_raw("%d", p->x);
    }
    log_raw("];\n");

    points_offset = -1;
    points_ptr = points;
    log_raw("py = [");
    for (int i = 0; i < 199; ++i)
    {
        if (i - points_offset >= (int)vector_count(points_ptr) - 1)
        {
            points_offset = i;
            points_ptr++;
        }

        printf("%d ", i);
        qwpos* p = (qwpos*)vector_get(points_ptr, i - points_offset);

        if (i != 0)
            log_raw(", ");
        log_raw("%d", p->y);
    }
    log_raw("];\n");
}

TEST(NAME, roll_back_over_frame_boundary)
{
    struct snake client, server;
    snake_init(&client, make_qwposi(2, 2), "client");
    snake_init(&server, make_qwposi(2, 2), "server");

    struct snake_param param;
    snake_param_init(&param);
    param.base_stats.turn_speed = make_qa2(1, 16);
    param.base_stats.min_speed = make_qw2(1, 256);
    param.base_stats.max_speed = make_qw2(1, 128);
    param.base_stats.boost_speed = make_qw2(1, 64);
    param.base_stats.acceleration = 8;
    snake_param_update(&param, {}, 1024);

    struct command c = command_default();

    uint16_t frame_number = 65535 - 10;
    uint16_t mispredict_frame = frame_number + 4;
    for (int i = 0; i < 200; ++i)
    {
        c.angle += 2;
        command_rb_put(&client.command_rb, c, frame_number);
        snake_step(&client.data, &client.head, &param, c, 60);

        if (u16_le_wrap(frame_number, mispredict_frame))
        {
            snake_step(&server.data, &server.head, &param, c, 60);
            if (frame_number == mispredict_frame)
                snake_step(&server.data, &server.head, &param, c, 60);  /* mispredict */
        }

        frame_number++;
    }
    mispredict_frame++;

    /* Make sure we have 7 bezier segments */
    ASSERT_THAT(rb_count(&client.data.points_lists), Eq(7));
    struct cs_vector* client_pts = (cs_vector*)rb_peek(&client.data.points_lists, 0);
    ASSERT_THAT(vector_count(&client_pts[0]), Eq(10));
    ASSERT_THAT(vector_count(&client_pts[1]), Eq(36));
    ASSERT_THAT(vector_count(&client_pts[2]), Eq(33));
    ASSERT_THAT(vector_count(&client_pts[3]), Eq(33));
    ASSERT_THAT(vector_count(&client_pts[4]), Eq(33));
    ASSERT_THAT(vector_count(&client_pts[5]), Eq(33));
    ASSERT_THAT(vector_count(&client_pts[6]), Eq(29));
    ASSERT_THAT(rb_count(&client.data.bezier_handles), Eq(8));

    print_points_lists(client_pts);

    ASSERT_THAT(rb_count(&server.data.points_lists), Eq(1));
    struct cs_vector* server_pts = (cs_vector*)rb_peek(&server.data.points_lists, 0);
    ASSERT_THAT(vector_count(&server_pts[0]), Eq(7));
    ASSERT_THAT(rb_count(&server.data.bezier_handles), Eq(2));

    /* Make sure sim agrees up to mispredicted frame */
    ASSERT_THAT(
        ((qwpos*)vector_get(&client_pts[0], 5))->x,
        Eq(((qwpos*)vector_get(&server_pts[0], 5))->x));
    ASSERT_THAT(
        ((qwpos*)vector_get(&client_pts[0], 5))->y,
        Eq(((qwpos*)vector_get(&server_pts[0], 5))->y));
    ASSERT_THAT(
        ((qwpos*)vector_get(&client_pts[0], 6))->x,
        Ne(((qwpos*)vector_get(&server_pts[0], 6))->x));
    ASSERT_THAT(
        ((qwpos*)vector_get(&client_pts[0], 6))->y,
        Ne(((qwpos*)vector_get(&server_pts[0], 6))->y));

    /* Everything is set up so that "mispredict_frame" is the last frame on which
     * the simulation will match up. Going from mispredict_frame to mispredict_frame+1
     * will cause a roll back */
    snake_ack_frame(&client.data, &client.head_ack, &client.head, &server.head, &param, &client.command_rb, mispredict_frame, 60);

    ASSERT_THAT(rb_count(&client.data.points_lists), Eq(7));
    client_pts = (cs_vector*)rb_peek(&client.data.points_lists, 0);
    ASSERT_THAT(rb_count(&client.data.bezier_handles), Eq(8));

    print_points_lists(client_pts);

    struct command c_prev = c;
    c = command_default();
    struct snake_head head;
    snake_head_init(&head, make_qwposi(2, 2));
    frame_number = 65535 - 10;
    int points_offset = -1;
    for (int i = 0; i < 199; ++i)
    {
        c.angle += 2;

        if (frame_number != mispredict_frame)
            snake_step_head(&head, &param, c, 60);
        else
            snake_step_head(&head, &param, c_prev, 60);

        if (i - points_offset >= (int)vector_count(client_pts) - 1)
        {
            points_offset = i;
            client_pts++;
        }

        printf("%d ", i);
        qwpos* p = (qwpos*)vector_get(client_pts, i - points_offset);

        ASSERT_THAT(head.pos.x, Eq(p->x));
        ASSERT_THAT(head.pos.y, Eq(p->y));

        frame_number++;
        c_prev = c;
    }

    snake_deinit(&client);
    snake_deinit(&server);
}
