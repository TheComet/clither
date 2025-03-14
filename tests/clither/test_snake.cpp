#include "gmock/gmock.h"

extern "C" {
#include "clither/bezier_handle_rb.h"
#include "clither/log.h"
#include "clither/qwaabb_rb.h"
#include "clither/qwpos_vec.h"
#include "clither/qwpos_vec_rb.h"
#include "clither/snake.h"
#include "clither/vec.h"
#include "clither/wrap.h"
}

#define NAME snake

using namespace testing;

static void print_head_trails(const struct qwpos_vec_rb* rb)
{
    int                      comma = 0;
    int                      i;
    struct qwpos_vec* const* points;
    struct qwpos*            p;
    log_raw("px = [");
    rb_for_each (rb, i, points)
        vec_for_each (*points, p)
        {
            if (comma)
                log_raw(", ");
            log_raw("%d", p->x);
            comma = 1;
        }
    log_raw("];\n");

    comma = 0;
    log_raw("py = [");
    rb_for_each (rb, i, points)
        vec_for_each (*points, p)
        {
            if (comma)
                log_raw(", ");
            log_raw("%d", p->y);
            comma = 1;
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

    struct cmd c = cmd_default();

    uint16_t frame_number = 65535 - 10;
    uint16_t mispredict_frame = frame_number + 4;
    for (int i = 0; i < 200; ++i)
    {
        c.angle += 2;
        cmd_queue_put(&client.cmdq, c, frame_number);
        snake_step(&client.data, &client.head, &param, c, 60);

        if (u16_le_wrap(frame_number, mispredict_frame))
        {
            snake_step(&server.data, &server.head, &param, c, 60);
            if (frame_number == mispredict_frame)
                snake_step(
                    &server.data, &server.head, &param, c, 60); /* mispredict */
        }

        frame_number++;
    }
    mispredict_frame++;

    /* Make sure we have 7 bezier segments */
    ASSERT_THAT(rb_count(client.data.head_trails), Eq(7u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 0)), Eq(10u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 1)), Eq(36u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 2)), Eq(33u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 3)), Eq(33u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 4)), Eq(33u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 5)), Eq(33u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 6)), Eq(29u));
    ASSERT_THAT(rb_count(client.data.bezier_handles), Eq(8u));

    ASSERT_THAT(rb_count(server.data.head_trails), Eq(1u));
    ASSERT_THAT(vec_count(*rb_peek(server.data.head_trails, 0)), Eq(7u));
    ASSERT_THAT(rb_count(server.data.bezier_handles), Eq(2u));

    /* Make sure sim agrees up to mispredicted frame */
    struct qwpos_vec* client_pts = *rb_peek(client.data.head_trails, 0);
    struct qwpos_vec* server_pts = *rb_peek(server.data.head_trails, 0);
    ASSERT_THAT(vec_get(client_pts, 5)->x, Eq(vec_get(server_pts, 5)->x));
    ASSERT_THAT(vec_get(client_pts, 5)->y, Eq(vec_get(server_pts, 5)->y));
    ASSERT_THAT(vec_get(client_pts, 6)->x, Ne(vec_get(server_pts, 6)->x));
    ASSERT_THAT(vec_get(client_pts, 6)->y, Ne(vec_get(server_pts, 6)->y));

    /* Everything is set up so that "mispredict_frame" is the last frame on
     * which the simulation will match up. Going from mispredict_frame to
     * mispredict_frame+1 will cause a roll back */
    snake_ack_frame(
        &client.data,
        &client.head_ack,
        &client.head,
        &server.head,
        &param,
        &client.cmdq,
        mispredict_frame,
        60);

    ASSERT_THAT(rb_count(client.data.head_trails), Eq(7u));
    client_pts = *rb_peek(client.data.head_trails, 0);
    ASSERT_THAT(rb_count(client.data.bezier_handles), Eq(8u));

    struct cmd c_prev = c;
    c = cmd_default();
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

        if (i - points_offset >= vec_count(client_pts) - 1)
        {
            points_offset = i;
            client_pts++;
        }

        qwpos* p = (qwpos*)vec_get(client_pts, i - points_offset);

        ASSERT_THAT(head.pos.x, Eq(p->x));
        ASSERT_THAT(head.pos.y, Eq(p->y));

        frame_number++;
        c_prev = c;
    }

    snake_deinit(&client);
    snake_deinit(&server);
}

TEST(NAME, roll_back_with_server_packet_loss)
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

    struct cmd c = cmd_default();

    uint16_t frame_number = 65535 - 10;
    uint16_t mispredict_frame = frame_number + 4;
    for (int i = 0; i < 200; ++i)
    {
        c.angle += 2;
        cmd_queue_put(&client.cmdq, c, frame_number);
        snake_step(&client.data, &client.head, &param, c, 60);

        if (u16_le_wrap(frame_number, mispredict_frame))
        {
            snake_step(&server.data, &server.head, &param, c, 60);
            if (frame_number == mispredict_frame)
            {
                /* mispredict a few frames*/
                int j;
                for (j = 0; j != 4; ++j)
                    snake_step(&server.data, &server.head, &param, c, 60);
            }
        }

        frame_number++;
    }
    mispredict_frame++;

    // Make sure we have 7 bezier segments
    ASSERT_THAT(rb_count(client.data.head_trails), Eq(7u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 0)), Eq(10u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 1)), Eq(36u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 2)), Eq(33u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 3)), Eq(33u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 4)), Eq(33u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 5)), Eq(33u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 6)), Eq(29u));
    ASSERT_THAT(rb_count(client.data.bezier_handles), Eq(8u));

    ASSERT_THAT(rb_count(server.data.head_trails), Eq(1u));
    ASSERT_THAT(vec_count(*rb_peek(server.data.head_trails, 0)), Eq(10u));
    ASSERT_THAT(rb_count(server.data.bezier_handles), Eq(2u));

    print_head_trails(server.data.head_trails);

    /* Make sure sim agrees up to mispredicted frame */
    struct qwpos_vec* client_pts = *rb_peek(client.data.head_trails, 0);
    struct qwpos_vec* server_pts = *rb_peek(server.data.head_trails, 0);
    ASSERT_THAT(vec_get(client_pts, 5)->x, Eq(vec_get(server_pts, 5)->x));
    ASSERT_THAT(vec_get(client_pts, 5)->y, Eq(vec_get(server_pts, 5)->y));
    ASSERT_THAT(vec_get(client_pts, 6)->x, Ne(vec_get(server_pts, 6)->x));
    ASSERT_THAT(vec_get(client_pts, 6)->y, Ne(vec_get(server_pts, 6)->y));

    print_head_trails(client.data.head_trails);

    /* Everything is set up so that "mispredict_frame" is the last frame on
     * which the simulation will match up. Going from mispredict_frame to
     * mispredict_frame+1 will cause a roll back */
    snake_ack_frame(
        &client.data,
        &client.head_ack,
        &client.head,
        &server.head,
        &param,
        &client.cmdq,
        mispredict_frame + 4,
        60);

    print_head_trails(client.data.head_trails);

    ASSERT_THAT(rb_count(client.data.head_trails), Eq(7u));
    client_pts = *rb_peek(client.data.head_trails, 0);
    ASSERT_THAT(rb_count(client.data.bezier_handles), Eq(8u));

    struct cmd c_mispredict = c;
    c = cmd_default();
    struct snake_head head;
    snake_head_init(&head, make_qwposi(2, 2));
    frame_number = 65535 - 10;
    int points_offset = -1;
    for (int i = 0; i < 199; ++i)
    {
        c.angle += 2;

        if (frame_number == (uint16_t)(mispredict_frame - 1))
            c_mispredict = c;

        if (u16_lt_wrap(frame_number, mispredict_frame) ||
            u16_gt_wrap(frame_number + 4, mispredict_frame))
            snake_step_head(&head, &param, c, 60);
        else
            snake_step_head(&head, &param, c_mispredict, 60);

        if (i - points_offset >= vec_count(client_pts) - 1)
        {
            points_offset = i;
            client_pts++;
        }

        qwpos* p = (qwpos*)vec_get(client_pts, i - points_offset);

        ASSERT_THAT(head.pos.x, Eq(p->x));
        ASSERT_THAT(head.pos.y, Eq(p->y));

        frame_number++;
    }

    snake_deinit(&client);
    snake_deinit(&server);
}

TEST(NAME, roll_back_to_first_frame)
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

    struct cmd c = cmd_default();

    uint16_t frame_number = 65535 - 10;
    snake_step(&server.data, &server.head, &param, c, 60);
    for (int i = 0; i < 200; ++i)
    {
        c.angle += 2;
        cmd_queue_put(&client.cmdq, c, frame_number);
        snake_step(&client.data, &client.head, &param, c, 60);

        frame_number++;
    }

    ASSERT_THAT(rb_count(client.data.head_trails), Ge(1u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 0)), Ge(2u));
    ASSERT_THAT(rb_count(client.data.bezier_handles), Ge(2u));

    ASSERT_THAT(rb_count(server.data.head_trails), Ge(1u));
    ASSERT_THAT(vec_count(*rb_peek(server.data.head_trails, 0)), Ge(2u));
    ASSERT_THAT(rb_count(server.data.bezier_handles), Ge(2u));

    /* Make sure sim agrees up to mispredicted frame */
    struct qwpos_vec* client_pts = *rb_peek(client.data.head_trails, 0);
    struct qwpos_vec* server_pts = *rb_peek(server.data.head_trails, 0);
    ASSERT_THAT(vec_get(client_pts, 0)->x, Eq(vec_get(server_pts, 0)->x));
    ASSERT_THAT(vec_get(client_pts, 0)->y, Eq(vec_get(server_pts, 0)->y));
    ASSERT_THAT(vec_get(client_pts, 1)->y, Ne(vec_get(server_pts, 1)->y));

    /* Everything is set up so that "mispredict_frame" is the last frame on
     * which the simulation will match up. Going from mispredict_frame to
     * mispredict_frame+1 will cause a roll back */
    snake_ack_frame(
        &client.data,
        &client.head_ack,
        &client.head,
        &server.head,
        &param,
        &client.cmdq,
        65535 - 10,
        60);

    snake_deinit(&client);
    snake_deinit(&server);
}

TEST(NAME, ackd_head_is_never_outside_aabb)
{
    struct snake client, server;
    snake_init(&client, make_qwposi(1, 1), "client");
    snake_init(&server, make_qwposi(1, 1), "server");

    struct snake_param param;
    snake_param_init(&param);
    param.base_stats.turn_speed = make_qa2(1, 16);
    param.base_stats.min_speed = make_qw2(1, 256);
    param.base_stats.max_speed = make_qw2(1, 128);
    param.base_stats.boost_speed = make_qw2(1, 64);
    param.base_stats.acceleration = 8;
    snake_param_update(&param, {}, 1);

    struct cmd c = cmd_default();
    uint16_t   frame_number = 65535 - 10;
    for (int i = 0; i < 75; ++i, ++frame_number)
    {
        c.angle += 2;
        cmd_queue_put(&client.cmdq, c, frame_number);
        snake_step(&client.data, &client.head, &param, c, 60);
    }

    // Make sure we have 7 bezier segments
    ASSERT_THAT(rb_count(client.data.head_trails), Eq(3u));
    ASSERT_THAT(rb_count(client.data.bezier_handles), Eq(4u));
    ASSERT_THAT(rb_count(client.data.bezier_aabbs), Eq(3u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 0)), Eq(10u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 1)), Eq(36u));
    ASSERT_THAT(vec_count(*rb_peek(client.data.head_trails, 2)), Eq(32u));

    // Reset same conditions for stepping server snake
    c = cmd_default();
    frame_number = 65535 - 10;

    // ------------------------------------------------------------------------
    // Step ack'd head up until 1 point before the end of the 1st segment
    for (int i = 0; i < 9; ++i, ++frame_number)
    {
        c.angle += 2;
        snake_step(&server.data, &server.head, &param, c, 60);
        snake_ack_frame(
            &client.data,
            &client.head_ack,
            &client.head,
            &server.head,
            &param,
            &client.cmdq,
            frame_number,
            60);

        ASSERT_THAT(rb_count(client.data.head_trails), Eq(3u));
        ASSERT_THAT(rb_count(client.data.bezier_handles), Eq(4u));
        ASSERT_THAT(rb_count(client.data.bezier_aabbs), Eq(3u));
        // If this is ever false, it means the bounding box of the curve does
        // not contain the acknowledged head position. The method used to
        // calculate the AABB is therefore incorrect.
        ASSERT_THAT(
            qwaabb_test_qwpos(
                *rb_peek(client.data.bezier_aabbs, 0), client.head_ack.pos),
            IsTrue());
    }
    // Trying to remove the segment should fail, because the ack'd head is still
    // within the bounding box
    snake_remove_stale_segments_with_rollback_constraint(
        &client.data, &client.head_ack, 1);
    ASSERT_THAT(rb_count(client.data.head_trails), Eq(3u));
    ASSERT_THAT(rb_count(client.data.bezier_handles), Eq(4u));
    ASSERT_THAT(rb_count(client.data.bezier_aabbs), Eq(3u));

    // Next step should remove the segment
    c.angle += 2;
    snake_step(&server.data, &server.head, &param, c, 60);
    snake_ack_frame(
        &client.data,
        &client.head_ack,
        &client.head,
        &server.head,
        &param,
        &client.cmdq,
        frame_number,
        60);
    frame_number++;

    ASSERT_THAT(rb_count(client.data.head_trails), Eq(3u));
    ASSERT_THAT(rb_count(client.data.bezier_handles), Eq(4u));
    ASSERT_THAT(rb_count(client.data.bezier_aabbs), Eq(3u));
    ASSERT_THAT(
        qwaabb_test_qwpos(
            *rb_peek(client.data.bezier_aabbs, 0), client.head_ack.pos),
        IsFalse());
    snake_remove_stale_segments_with_rollback_constraint(
        &client.data, &client.head_ack, 1);
    ASSERT_THAT(rb_count(client.data.head_trails), Eq(2u));
    ASSERT_THAT(rb_count(client.data.bezier_handles), Eq(3u));
    ASSERT_THAT(rb_count(client.data.bezier_aabbs), Eq(2u));

    // ------------------------------------------------------------------------
    // Step ack'd head up until 1 point before the end of the 2nd segment
    for (int i = 0; i < 34; ++i, ++frame_number)
    {
        c.angle += 2;
        snake_step(&server.data, &server.head, &param, c, 60);
        snake_ack_frame(
            &client.data,
            &client.head_ack,
            &client.head,
            &server.head,
            &param,
            &client.cmdq,
            frame_number,
            60);

        ASSERT_THAT(rb_count(client.data.head_trails), Eq(2u));
        ASSERT_THAT(rb_count(client.data.bezier_handles), Eq(3u));
        ASSERT_THAT(rb_count(client.data.bezier_aabbs), Eq(2u));
        // If this is ever false, it means the bounding box of the curve does
        // not contain the acknowledged head position. The method used to
        // calculate the AABB is therefore incorrect.
        ASSERT_THAT(
            qwaabb_test_qwpos(
                *rb_peek(client.data.bezier_aabbs, 0), client.head_ack.pos),
            IsTrue());
    }
    // Trying to remove the segment should fail, because the ack'd head is still
    // within the bounding box
    snake_remove_stale_segments_with_rollback_constraint(
        &client.data, &client.head_ack, 1);
    ASSERT_THAT(rb_count(client.data.head_trails), Eq(2u));
    ASSERT_THAT(rb_count(client.data.bezier_handles), Eq(3u));
    ASSERT_THAT(rb_count(client.data.bezier_aabbs), Eq(2u));

    // Next step should remove the segment
    c.angle += 2;
    snake_step(&server.data, &server.head, &param, c, 60);
    snake_ack_frame(
        &client.data,
        &client.head_ack,
        &client.head,
        &server.head,
        &param,
        &client.cmdq,
        frame_number,
        60);
    frame_number++;

    ASSERT_THAT(rb_count(client.data.head_trails), Eq(2u));
    ASSERT_THAT(rb_count(client.data.bezier_handles), Eq(3u));
    ASSERT_THAT(rb_count(client.data.bezier_aabbs), Eq(2u));
    ASSERT_THAT(
        qwaabb_test_qwpos(
            *rb_peek(client.data.bezier_aabbs, 0), client.head_ack.pos),
        IsFalse());
    snake_remove_stale_segments_with_rollback_constraint(
        &client.data, &client.head_ack, 1);
    ASSERT_THAT(rb_count(client.data.head_trails), Eq(1u));
    ASSERT_THAT(rb_count(client.data.bezier_handles), Eq(2u));
    ASSERT_THAT(rb_count(client.data.bezier_aabbs), Eq(1u));

    // ------------------------------------------------------------------------
    // Step ack'd head up until 1 point before the end of the 3nd segment
    for (int i = 0; i < 30; ++i, ++frame_number)
    {
        c.angle += 2;
        snake_step(&server.data, &server.head, &param, c, 60);
        snake_ack_frame(
            &client.data,
            &client.head_ack,
            &client.head,
            &server.head,
            &param,
            &client.cmdq,
            frame_number,
            60);

        ASSERT_THAT(rb_count(client.data.head_trails), Eq(1u));
        ASSERT_THAT(rb_count(client.data.bezier_handles), Eq(2u));
        ASSERT_THAT(rb_count(client.data.bezier_aabbs), Eq(1u));
        ASSERT_THAT(
            qwaabb_test_qwpos(
                *rb_peek(client.data.bezier_aabbs, 0), client.head_ack.pos),
            IsTrue());
    }

    snake_deinit(&server);
    snake_deinit(&client);
}
