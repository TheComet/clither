#include "gmock/gmock.h"

extern "C" {
#include "clither/cmd.h"
#include "clither/snake.h"
}

#define NAME command

using namespace testing;

TEST(NAME, default_command_agrees_with_default_snake_head)
{
    struct cmd         command;
    struct snake_head  head1, head2;
    struct snake_param param;
    command = cmd_default();
    snake_head_init(&head1, make_qwposi(0, 0));
    snake_head_init(&head2, make_qwposi(0, 0));
    snake_param_init(&param);

    snake_step_head(&head2, &param, command, 60);
    EXPECT_THAT(head1.angle, Eq(head2.angle));
}

TEST(NAME, inserting_sequential_commands_works)
{
    struct cmd       c = cmd_default();
    struct cmd_queue cmdq;
    cmd_queue_init(&cmdq);

    c.angle = 1;
    cmd_queue_put(&cmdq, c, 65534);
    c.angle = 2;
    cmd_queue_put(&cmdq, c, 65535);
    c.angle = 3;
    cmd_queue_put(&cmdq, c, 0);
    c.angle = 4;
    cmd_queue_put(&cmdq, c, 1);
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(4));
    EXPECT_THAT(cmd_queue_peek(&cmdq, 0)->angle, Eq(1));
    EXPECT_THAT(cmd_queue_peek(&cmdq, 1)->angle, Eq(2));
    EXPECT_THAT(cmd_queue_peek(&cmdq, 2)->angle, Eq(3));
    EXPECT_THAT(cmd_queue_peek(&cmdq, 3)->angle, Eq(4));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(65534));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(2));

    cmd_queue_deinit(&cmdq);
}

TEST(NAME, inserting_with_frame_gaps_doesnt_work)
{
    struct cmd       c = cmd_default();
    struct cmd_queue cmdq;
    cmd_queue_init(&cmdq);

    c.angle = 1;
    cmd_queue_put(&cmdq, c, 65534);
    c.angle = 2;
    cmd_queue_put(&cmdq, c, 65535);
    c.angle = 3;
    cmd_queue_put(&cmdq, c, 1);
    c.angle = 4;
    cmd_queue_put(&cmdq, c, 2);
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(2));
    EXPECT_THAT(cmd_queue_peek(&cmdq, 0)->angle, Eq(1));
    EXPECT_THAT(cmd_queue_peek(&cmdq, 1)->angle, Eq(2));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(65534));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(0));

    cmd_queue_deinit(&cmdq);
}

TEST(NAME, take_first_matching_command)
{
    struct cmd       c = cmd_default();
    struct cmd_queue cmdq;
    cmd_queue_init(&cmdq);

    c.angle = 1;
    cmd_queue_put(&cmdq, c, 65534);
    c.angle = 2;
    cmd_queue_put(&cmdq, c, 65535);
    c.angle = 3;
    cmd_queue_put(&cmdq, c, 0);
    c.angle = 4;
    cmd_queue_put(&cmdq, c, 1);
    c.angle = 5;
    cmd_queue_put(&cmdq, c, 2);
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(5));

    EXPECT_THAT(cmd_queue_find_or_predict(&cmdq, 65534).angle, Eq(1));
    EXPECT_THAT(cmd_queue_find_or_predict(&cmdq, 65534).angle, Eq(1));
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(5));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(65534));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(3));

    EXPECT_THAT(cmd_queue_take_or_predict(&cmdq, 65534).angle, Eq(1));
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(4));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(65535));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(3));

    EXPECT_THAT(cmd_queue_take_or_predict(&cmdq, 65535).angle, Eq(2));
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(3));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(0));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(3));

    EXPECT_THAT(cmd_queue_take_or_predict(&cmdq, 0).angle, Eq(3));
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(2));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(1));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(3));

    EXPECT_THAT(cmd_queue_take_or_predict(&cmdq, 1).angle, Eq(4));
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(1));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(2));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(3));

    EXPECT_THAT(cmd_queue_take_or_predict(&cmdq, 2).angle, Eq(5));
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(0));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(3));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(3));

    cmd_queue_deinit(&cmdq);
}

TEST(NAME, take_command_in_past_uses_last_predicted)
{
    struct cmd       c = cmd_default();
    struct cmd_queue cmdq;
    cmd_queue_init(&cmdq);

    c.angle = 2;
    cmd_queue_put(&cmdq, c, 65535);
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(1));

    EXPECT_THAT(
        cmd_queue_find_or_predict(&cmdq, 65530).angle, Eq(cmd_default().angle));
    EXPECT_THAT(
        cmd_queue_find_or_predict(&cmdq, 65533).angle, Eq(cmd_default().angle));
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(1));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(65535));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(0));

    EXPECT_THAT(
        cmd_queue_take_or_predict(&cmdq, 65530).angle, Eq(cmd_default().angle));
    EXPECT_THAT(
        cmd_queue_take_or_predict(&cmdq, 65533).angle, Eq(cmd_default().angle));
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(1));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(65535));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(0));

    cmd_queue_deinit(&cmdq);
}

TEST(NAME, take_last_command_updates_predicted)
{
    struct cmd       c = cmd_default();
    struct cmd_queue cmdq;
    cmd_queue_init(&cmdq);

    c.angle = 2;
    cmd_queue_put(&cmdq, c, 65535);
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(1));

    // Should update last predicted command
    cmd_queue_take_or_predict(&cmdq, 65535);
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(0));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(0));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(0));

    EXPECT_THAT(cmd_queue_find_or_predict(&cmdq, 0).angle, Eq(2));
    EXPECT_THAT(cmd_queue_find_or_predict(&cmdq, 1).angle, Eq(2));
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(0));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(0));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(0));

    EXPECT_THAT(cmd_queue_take_or_predict(&cmdq, 0).angle, Eq(2));
    EXPECT_THAT(cmd_queue_take_or_predict(&cmdq, 1).angle, Eq(2));
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(0));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(0));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(0));

    cmd_queue_deinit(&cmdq);
}

TEST(NAME, take_command_in_middle)
{
    struct cmd       c = cmd_default();
    struct cmd_queue cmdq;
    cmd_queue_init(&cmdq);

    c.angle = 1;
    cmd_queue_put(&cmdq, c, 65534);
    c.angle = 2;
    cmd_queue_put(&cmdq, c, 65535);
    c.angle = 3;
    cmd_queue_put(&cmdq, c, 0);
    c.angle = 4;
    cmd_queue_put(&cmdq, c, 1);
    c.angle = 5;
    cmd_queue_put(&cmdq, c, 2);
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(5));

    EXPECT_THAT(cmd_queue_find_or_predict(&cmdq, 0).angle, Eq(3));
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(5));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(65534));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(3));

    EXPECT_THAT(cmd_queue_take_or_predict(&cmdq, 0).angle, Eq(3));
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(2));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(1));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(3));

    cmd_queue_deinit(&cmdq);
}

TEST(NAME, take_command_in_future_clears_buffer)
{
    struct cmd       c = cmd_default();
    struct cmd_queue cmdq;
    cmd_queue_init(&cmdq);

    c.angle = 1;
    cmd_queue_put(&cmdq, c, 65534);
    c.angle = 2;
    cmd_queue_put(&cmdq, c, 65535);
    c.angle = 3;
    cmd_queue_put(&cmdq, c, 0);
    c.angle = 4;
    cmd_queue_put(&cmdq, c, 1);
    c.angle = 5;
    cmd_queue_put(&cmdq, c, 2);
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(5));

    EXPECT_THAT(cmd_queue_find_or_predict(&cmdq, 10).angle, Eq(5));
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(5));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(65534));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(3));

    EXPECT_THAT(cmd_queue_take_or_predict(&cmdq, 5).angle, Eq(5));
    EXPECT_THAT(cmd_queue_count(&cmdq), Eq(0));
    EXPECT_THAT(cmd_queue_frame_begin(&cmdq), Eq(3));
    EXPECT_THAT(cmd_queue_frame_end(&cmdq), Eq(3));

    cmd_queue_deinit(&cmdq);
}
