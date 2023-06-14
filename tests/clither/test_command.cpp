#include "gmock/gmock.h"
#include "clither/command.h"
#include "clither/snake.h"

#define NAME command

using namespace testing;

TEST(NAME, default_command_agree_with_default_snake_head)
{
    struct command command;
    struct snake_head head1, head2;
    command = command_default();
    snake_head_init(&head1, make_qwposi(0, 0));
    snake_head_init(&head2, make_qwposi(0, 0));

    snake_step_head(&head2, command, 60);
    EXPECT_THAT(head1.angle, Eq(head2.angle));
}

TEST(NAME, inserting_sequential_commands_works)
{
    struct command c = command_default();
    struct command_rb rb;
    command_rb_init(&rb);

    c.angle = 1;  command_rb_put(&rb, c, 65534);
    c.angle = 2;  command_rb_put(&rb, c, 65535);
    c.angle = 3;  command_rb_put(&rb, c, 0);
    c.angle = 4;  command_rb_put(&rb, c, 1);
    EXPECT_THAT(command_rb_count(&rb), Eq(4));
    EXPECT_THAT(command_rb_peek(&rb, 0)->angle, Eq(1));
    EXPECT_THAT(command_rb_peek(&rb, 1)->angle, Eq(2));
    EXPECT_THAT(command_rb_peek(&rb, 2)->angle, Eq(3));
    EXPECT_THAT(command_rb_peek(&rb, 3)->angle, Eq(4));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65534));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(2));

    command_rb_deinit(&rb);
}

TEST(NAME, inserting_with_frame_gaps_doesnt_work)
{
    struct command c = command_default();
    struct command_rb rb;
    command_rb_init(&rb);

    c.angle = 1;  command_rb_put(&rb, c, 65534);
    c.angle = 2;  command_rb_put(&rb, c, 65535);
    c.angle = 3;  command_rb_put(&rb, c, 1);
    c.angle = 4;  command_rb_put(&rb, c, 2);
    EXPECT_THAT(command_rb_count(&rb), Eq(2));
    EXPECT_THAT(command_rb_peek(&rb, 0)->angle, Eq(1));
    EXPECT_THAT(command_rb_peek(&rb, 1)->angle, Eq(2));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65534));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(0));

    command_rb_deinit(&rb);
}

TEST(NAME, take_first_matching_command)
{
    struct command c = command_default();
    struct command_rb rb;
    command_rb_init(&rb);

    c.angle = 1;  command_rb_put(&rb, c, 65534);
    c.angle = 2;  command_rb_put(&rb, c, 65535);
    c.angle = 3;  command_rb_put(&rb, c, 0);
    c.angle = 4;  command_rb_put(&rb, c, 1);
    c.angle = 5;  command_rb_put(&rb, c, 2);
    EXPECT_THAT(command_rb_count(&rb), Eq(5));

    EXPECT_THAT(command_rb_find_or_predict(&rb, 65534).angle, Eq(1));
    EXPECT_THAT(command_rb_find_or_predict(&rb, 65534).angle, Eq(1));
    EXPECT_THAT(command_rb_count(&rb), Eq(5));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65534));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 65534).angle, Eq(1));
    EXPECT_THAT(command_rb_count(&rb), Eq(4));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65535));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 65535).angle, Eq(2));
    EXPECT_THAT(command_rb_count(&rb), Eq(3));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(0));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 0).angle, Eq(3));
    EXPECT_THAT(command_rb_count(&rb), Eq(2));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(1));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 1).angle, Eq(4));
    EXPECT_THAT(command_rb_count(&rb), Eq(1));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(2));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 2).angle, Eq(5));
    EXPECT_THAT(command_rb_count(&rb), Eq(0));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(3));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    command_rb_deinit(&rb);
}

TEST(NAME, take_command_in_past_uses_last_predicted)
{
    struct command c = command_default();
    struct command_rb rb;
    command_rb_init(&rb);

    c.angle = 2;  command_rb_put(&rb, c, 65535);
    EXPECT_THAT(command_rb_count(&rb), Eq(1));

    EXPECT_THAT(command_rb_find_or_predict(&rb, 65530).angle, Eq(command_default().angle));
    EXPECT_THAT(command_rb_find_or_predict(&rb, 65533).angle, Eq(command_default().angle));
    EXPECT_THAT(command_rb_count(&rb), Eq(1));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65535));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(0));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 65530).angle, Eq(command_default().angle));
    EXPECT_THAT(command_rb_take_or_predict(&rb, 65533).angle, Eq(command_default().angle));
    EXPECT_THAT(command_rb_count(&rb), Eq(1));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65535));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(0));

    command_rb_deinit(&rb);
}

TEST(NAME, take_last_command_updates_predicted)
{
    struct command c = command_default();
    struct command_rb rb;
    command_rb_init(&rb);

    c.angle = 2;  command_rb_put(&rb, c, 65535);
    EXPECT_THAT(command_rb_count(&rb), Eq(1));

    // Should update last predicted command
    command_rb_take_or_predict(&rb, 65535);
    EXPECT_THAT(command_rb_count(&rb), Eq(0));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(0));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(0));

    EXPECT_THAT(command_rb_find_or_predict(&rb, 0).angle, Eq(2));
    EXPECT_THAT(command_rb_find_or_predict(&rb, 1).angle, Eq(2));
    EXPECT_THAT(command_rb_count(&rb), Eq(0));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(0));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(0));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 0).angle, Eq(2));
    EXPECT_THAT(command_rb_take_or_predict(&rb, 1).angle, Eq(2));
    EXPECT_THAT(command_rb_count(&rb), Eq(0));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(0));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(0));

    command_rb_deinit(&rb);
}

TEST(NAME, take_command_in_middle)
{
    struct command c = command_default();
    struct command_rb rb;
    command_rb_init(&rb);

    c.angle = 1;  command_rb_put(&rb, c, 65534);
    c.angle = 2;  command_rb_put(&rb, c, 65535);
    c.angle = 3;  command_rb_put(&rb, c, 0);
    c.angle = 4;  command_rb_put(&rb, c, 1);
    c.angle = 5;  command_rb_put(&rb, c, 2);
    EXPECT_THAT(command_rb_count(&rb), Eq(5));

    EXPECT_THAT(command_rb_find_or_predict(&rb, 0).angle, Eq(3));
    EXPECT_THAT(command_rb_count(&rb), Eq(5));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65534));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 0).angle, Eq(3));
    EXPECT_THAT(command_rb_count(&rb), Eq(2));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(1));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    command_rb_deinit(&rb);
}

TEST(NAME, take_command_in_future_clears_buffer)
{
    struct command c = command_default();
    struct command_rb rb;
    command_rb_init(&rb);

    c.angle = 1;  command_rb_put(&rb, c, 65534);
    c.angle = 2;  command_rb_put(&rb, c, 65535);
    c.angle = 3;  command_rb_put(&rb, c, 0);
    c.angle = 4;  command_rb_put(&rb, c, 1);
    c.angle = 5;  command_rb_put(&rb, c, 2);
    EXPECT_THAT(command_rb_count(&rb), Eq(5));

    EXPECT_THAT(command_rb_find_or_predict(&rb, 10).angle, Eq(5));
    EXPECT_THAT(command_rb_count(&rb), Eq(5));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65534));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 5).angle, Eq(5));
    EXPECT_THAT(command_rb_count(&rb), Eq(0));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(3));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    command_rb_deinit(&rb);
}
