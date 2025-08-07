#include "clither/tests/LogHelper.hpp"

#include "gmock/gmock.h"

extern "C" {
#include "clither/game/bezier_knot_rb.h"
#include "clither/game/bezier_segment_rb.h"
#include "clither/game/qwaabb_rb.h"
#include "clither/game/qwpos_vec.h"
#include "clither/game/qwpos_vec_rb.h"
#include "clither/game/snake.h"
#include "clither/game/wrap.h"
#include "clither/util/log.h"
#include "clither/util/vec.h"
}

#define NAME test_snake_rollback

using namespace testing;

struct NAME : Test, LogHelper
{
    void SetUp() override
    {
        snake_init(&client, make_qwposi(2, 2), "client");
        snake_init(&server, make_qwposi(2, 2), "server");
    }

    void TearDown() override
    {
        snake_deinit(&client);
        snake_deinit(&server);
    }

    int TotalPointsInTrail(const struct snake* s) const
    {
        int total = 0;
        for (int i = 0; i < rb_count(s->data.trails); ++i)
            total += vec_count(*rb_peek(s->data.trails, i));
        return total;
    }

    struct snake client, server;
};

TEST_F(NAME, roll_back_over_frame_boundary)
{
    snake_head_init(&client.remote.ack.head, make_qwposi(2, 2));

    struct snake_param param;
    snake_param_init(&param);
    param.base_stats.turn_speed = make_qa2(1, 16);
    param.base_stats.min_speed = make_qw2(1, 256);
    param.base_stats.max_speed = make_qw2(1, 128);
    param.base_stats.boost_speed = make_qw2(1, 64);
    param.base_stats.acceleration = 8;
    snake_param_update(&param, {}, 1024);

    struct cmd c = cmd_default();
    uint16_t   frame_number = 65535 - 10;
    uint16_t   mispredict_frame = frame_number + 4;
    for (int i = 0; i < 120; ++i)
    {
        c.angle += 2;
        cmd_queue_put(&client.cmdq, c, frame_number);
        snake_step(&client.data, &client.head, &param, c, 60);

        if (u16_le_wrap(frame_number, mispredict_frame))
        {
            snake_step(&server.data, &server.head, &param, c, 60);
            /* mispredict step, c_next = c */
            if (frame_number == mispredict_frame)
                snake_step(&server.data, &server.head, &param, c, 60);
        }

        frame_number++;
    }
    mispredict_frame++;

    /* Check to see we generated bezier curves */
    int segment_count = 4;
    ASSERT_THAT(rb_count(client.data.trails), Eq(segment_count));
    ASSERT_THAT(rb_count(client.data.knots), Eq(segment_count + 1));
    ASSERT_THAT(rb_count(client.data.segments), Eq(segment_count));
    ASSERT_THAT(rb_count(client.data.segment_bbs), Eq(segment_count));
    ASSERT_THAT(TotalPointsInTrail(&client), Eq(120 + segment_count));

    ASSERT_THAT(rb_count(server.data.trails), Eq(1));
    ASSERT_THAT(vec_count(*rb_peek(server.data.trails, 0)), Eq(7));
    ASSERT_THAT(rb_count(server.data.knots), Eq(2));

    /* Make sure sim agrees up to mispredicted frame */
    struct qwpos_vec* client_trails = *rb_peek(client.data.trails, 0);
    struct qwpos_vec* server_trails = *rb_peek(server.data.trails, 0);
    ASSERT_THAT(vec_get(client_trails, 5)->x, Eq(vec_get(server_trails, 5)->x));
    ASSERT_THAT(vec_get(client_trails, 5)->y, Eq(vec_get(server_trails, 5)->y));
    ASSERT_THAT(vec_get(client_trails, 6)->x, Ne(vec_get(server_trails, 6)->x));
    ASSERT_THAT(vec_get(client_trails, 6)->y, Ne(vec_get(server_trails, 6)->y));

    /* Everything is set up so that "mispredict_frame" is the last frame on
     * which the simulation will match up. Going from mispredict_frame to
     * mispredict_frame+1 will cause a roll back */
    snake_ack_frame(
        &client.data,
        &client.remote.ack,
        &client.head,
        &server.head,
        &param,
        &client.cmdq,
        mispredict_frame,
        60);

    /* Did the client roll forward again? */
    ASSERT_THAT(rb_count(client.data.trails), Eq(segment_count));
    ASSERT_THAT(rb_count(client.data.knots), Eq(segment_count + 1));
    ASSERT_THAT(rb_count(client.data.segments), Eq(segment_count));
    ASSERT_THAT(rb_count(client.data.segment_bbs), Eq(segment_count));
    ASSERT_THAT(TotalPointsInTrail(&client), Eq(120 + segment_count));

    int trail_idx = 0;
    int point_idx = 0;
    c = cmd_default();
    struct snake_head head;
    snake_head_init(&head, make_qwposi(2, 2));
    frame_number = 65535 - 10;
    for (int i = 0; i < 120; ++i)
    {
        c.angle += 2;
        if (frame_number != mispredict_frame)
            snake_step_head(&head, &param, c, 60);
        else
        {
            c.angle -= 2;
            snake_step_head(&head, &param, c, 60);
            c.angle += 2;
        }

        point_idx++;
        const struct qwpos_vec* trail = *rb_peek(client.data.trails, trail_idx);
        if (point_idx >= vec_count(trail))
        {
            ++trail_idx;
            trail = *rb_peek(client.data.trails, trail_idx);
            point_idx = 1;
        }
        const qwpos* p = vec_get(trail, point_idx);

        ASSERT_THAT(head.pos.x, Eq(p->x)) << i;
        ASSERT_THAT(head.pos.y, Eq(p->y)) << i;

        frame_number++;
    }
}

TEST_F(NAME, roll_back_with_server_packet_loss)
{
    snake_head_init(&client.remote.ack.head, make_qwposi(2, 2));

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
    for (int i = 0; i < 120; ++i)
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

    /* Check to see we generated bezier curves */
    int segment_count = 4;
    ASSERT_THAT(rb_count(client.data.trails), Eq(segment_count));
    ASSERT_THAT(rb_count(client.data.knots), Eq(segment_count + 1));
    ASSERT_THAT(rb_count(client.data.segments), Eq(segment_count));
    ASSERT_THAT(rb_count(client.data.segment_bbs), Eq(segment_count));
    ASSERT_THAT(TotalPointsInTrail(&client), Eq(120 + segment_count));

    ASSERT_THAT(rb_count(server.data.trails), Eq(1));
    ASSERT_THAT(vec_count(*rb_peek(server.data.trails, 0)), Eq(10));
    ASSERT_THAT(rb_count(server.data.knots), Eq(2));

    /* Make sure sim agrees up to mispredicted frame */
    struct qwpos_vec* client_pts = *rb_peek(client.data.trails, 0);
    struct qwpos_vec* server_pts = *rb_peek(server.data.trails, 0);
    ASSERT_THAT(vec_get(client_pts, 5)->x, Eq(vec_get(server_pts, 5)->x));
    ASSERT_THAT(vec_get(client_pts, 5)->y, Eq(vec_get(server_pts, 5)->y));
    ASSERT_THAT(vec_get(client_pts, 6)->x, Ne(vec_get(server_pts, 6)->x));
    ASSERT_THAT(vec_get(client_pts, 6)->y, Ne(vec_get(server_pts, 6)->y));

    /* Everything is set up so that "mispredict_frame" is the last frame on
     * which the simulation will match up. Going from mispredict_frame to
     * mispredict_frame+1 will cause a roll back */
    snake_ack_frame(
        &client.data,
        &client.remote.ack,
        &client.head,
        &server.head,
        &param,
        &client.cmdq,
        mispredict_frame + 4,
        60);

    /* Did the client roll forward again? */
    segment_count = 6;
    ASSERT_THAT(rb_count(client.data.trails), Eq(segment_count));
    ASSERT_THAT(rb_count(client.data.knots), Eq(segment_count + 1));
    ASSERT_THAT(rb_count(client.data.segments), Eq(segment_count));
    ASSERT_THAT(rb_count(client.data.segment_bbs), Eq(segment_count));
    ASSERT_THAT(TotalPointsInTrail(&client), Eq(120 + segment_count));

    struct cmd c_mispredict = c;
    c = cmd_default();
    struct snake_head head;
    snake_head_init(&head, make_qwposi(2, 2));
    frame_number = 65535 - 10;
    int points_offset = -1;
    for (int i = 0; i < 120; ++i)
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
}

TEST_F(NAME, roll_back_to_first_frame)
{
    snake_head_init(&client.remote.ack.head, make_qwposi(2, 2));

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

    ASSERT_THAT(rb_count(client.data.trails), Ge(1));
    ASSERT_THAT(vec_count(*rb_peek(client.data.trails, 0)), Ge(2));
    ASSERT_THAT(rb_count(client.data.knots), Ge(2));

    ASSERT_THAT(rb_count(server.data.trails), Ge(1));
    ASSERT_THAT(vec_count(*rb_peek(server.data.trails, 0)), Ge(2));
    ASSERT_THAT(rb_count(server.data.knots), Ge(2));

    /* Make sure sim agrees up to mispredicted frame */
    struct qwpos_vec* client_pts = *rb_peek(client.data.trails, 0);
    struct qwpos_vec* server_pts = *rb_peek(server.data.trails, 0);
    ASSERT_THAT(vec_get(client_pts, 0)->x, Eq(vec_get(server_pts, 0)->x));
    ASSERT_THAT(vec_get(client_pts, 0)->y, Eq(vec_get(server_pts, 0)->y));
    ASSERT_THAT(vec_get(client_pts, 1)->y, Ne(vec_get(server_pts, 1)->y));

    /* Everything is set up so that "mispredict_frame" is the last frame on
     * which the simulation will match up. Going from mispredict_frame to
     * mispredict_frame+1 will cause a roll back */
    snake_ack_frame(
        &client.data,
        &client.remote.ack,
        &client.head,
        &server.head,
        &param,
        &client.cmdq,
        65535 - 10,
        60);
}

TEST_F(NAME, ackd_head_is_never_outside_aabb)
{
    snake_head_init(&client.remote.ack.head, make_qwposi(3, 3));

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
    ASSERT_THAT(rb_count(client.data.trails), Eq(3));
    ASSERT_THAT(rb_count(client.data.knots), Eq(4));
    ASSERT_THAT(rb_count(client.data.segment_bbs), Eq(3));
    ASSERT_THAT(vec_count(*rb_peek(client.data.trails, 0)), Eq(10));
    ASSERT_THAT(vec_count(*rb_peek(client.data.trails, 1)), Eq(36));
    ASSERT_THAT(vec_count(*rb_peek(client.data.trails, 2)), Eq(32));

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
            &client.remote.ack,
            &client.head,
            &server.head,
            &param,
            &client.cmdq,
            frame_number,
            60);

        ASSERT_THAT(rb_count(client.data.trails), Eq(3));
        ASSERT_THAT(rb_count(client.data.knots), Eq(4));
        ASSERT_THAT(rb_count(client.data.segment_bbs), Eq(3));
        // If this is ever false, it means the bounding box of the curve does
        // not contain the acknowledged head position. The method used to
        // calculate the AABB is therefore incorrect.
        ASSERT_THAT(
            qwaabb_test_qwpos(
                *rb_peek(client.data.segment_bbs, 0),
                client.remote.ack.head.pos),
            IsTrue());
    }
    // Trying to remove the segment should fail, because the ack'd head is still
    // within the bounding box
    snake_remove_stale_segments_with_rollback_constraint(
        &client.data, &client.remote.ack, 1);
    ASSERT_THAT(rb_count(client.data.trails), Eq(3));
    ASSERT_THAT(rb_count(client.data.knots), Eq(4));
    ASSERT_THAT(rb_count(client.data.segment_bbs), Eq(3));

    // Next step should remove the segment
    c.angle += 2;
    snake_step(&server.data, &server.head, &param, c, 60);
    snake_ack_frame(
        &client.data,
        &client.remote.ack,
        &client.head,
        &server.head,
        &param,
        &client.cmdq,
        frame_number,
        60);
    frame_number++;

    ASSERT_THAT(rb_count(client.data.trails), Eq(3));
    ASSERT_THAT(rb_count(client.data.knots), Eq(4));
    ASSERT_THAT(rb_count(client.data.segment_bbs), Eq(3));
    ASSERT_THAT(
        qwaabb_test_qwpos(
            *rb_peek(client.data.segment_bbs, 0), client.remote.ack.head.pos),
        IsFalse());
    snake_remove_stale_segments_with_rollback_constraint(
        &client.data, &client.remote.ack, 1);
    ASSERT_THAT(rb_count(client.data.trails), Eq(2));
    ASSERT_THAT(rb_count(client.data.knots), Eq(3));
    ASSERT_THAT(rb_count(client.data.segment_bbs), Eq(2));

    // ------------------------------------------------------------------------
    // Step ack'd head up until 1 point before the end of the 2nd segment
    for (int i = 0; i < 34; ++i, ++frame_number)
    {
        c.angle += 2;
        snake_step(&server.data, &server.head, &param, c, 60);
        snake_ack_frame(
            &client.data,
            &client.remote.ack,
            &client.head,
            &server.head,
            &param,
            &client.cmdq,
            frame_number,
            60);

        ASSERT_THAT(rb_count(client.data.trails), Eq(2));
        ASSERT_THAT(rb_count(client.data.knots), Eq(3));
        ASSERT_THAT(rb_count(client.data.segment_bbs), Eq(2));
        // If this is ever false, it means the bounding box of the curve does
        // not contain the acknowledged head position. The method used to
        // calculate the AABB is therefore incorrect.
        ASSERT_THAT(
            qwaabb_test_qwpos(
                *rb_peek(client.data.segment_bbs, 0),
                client.remote.ack.head.pos),
            IsTrue());
    }
    // Trying to remove the segment should fail, because the ack'd head is still
    // within the bounding box
    snake_remove_stale_segments_with_rollback_constraint(
        &client.data, &client.remote.ack, 1);
    ASSERT_THAT(rb_count(client.data.trails), Eq(2));
    ASSERT_THAT(rb_count(client.data.knots), Eq(3));
    ASSERT_THAT(rb_count(client.data.segment_bbs), Eq(2));

    // Next step should remove the segment
    c.angle += 2;
    snake_step(&server.data, &server.head, &param, c, 60);
    snake_ack_frame(
        &client.data,
        &client.remote.ack,
        &client.head,
        &server.head,
        &param,
        &client.cmdq,
        frame_number,
        60);
    frame_number++;

    ASSERT_THAT(rb_count(client.data.trails), Eq(2));
    ASSERT_THAT(rb_count(client.data.knots), Eq(3));
    ASSERT_THAT(rb_count(client.data.segment_bbs), Eq(2));
    ASSERT_THAT(
        qwaabb_test_qwpos(
            *rb_peek(client.data.segment_bbs, 0), client.remote.ack.head.pos),
        IsFalse());
    snake_remove_stale_segments_with_rollback_constraint(
        &client.data, &client.remote.ack, 1);
    ASSERT_THAT(rb_count(client.data.trails), Eq(1));
    ASSERT_THAT(rb_count(client.data.knots), Eq(2));
    ASSERT_THAT(rb_count(client.data.segment_bbs), Eq(1));

    // ------------------------------------------------------------------------
    // Step ack'd head up until 1 point before the end of the 3nd segment
    for (int i = 0; i < 30; ++i, ++frame_number)
    {
        c.angle += 2;
        snake_step(&server.data, &server.head, &param, c, 60);
        snake_ack_frame(
            &client.data,
            &client.remote.ack,
            &client.head,
            &server.head,
            &param,
            &client.cmdq,
            frame_number,
            60);

        ASSERT_THAT(rb_count(client.data.trails), Eq(1));
        ASSERT_THAT(rb_count(client.data.knots), Eq(2));
        ASSERT_THAT(rb_count(client.data.segment_bbs), Eq(1));
        ASSERT_THAT(
            qwaabb_test_qwpos(
                *rb_peek(client.data.segment_bbs, 0),
                client.remote.ack.head.pos),
            IsTrue());
    }

    snake_deinit(&server);
    snake_deinit(&client);
}
