#include "gmock/gmock.h"
#include "clither/command.h"
#include "clither/snake.h"

#define NAME controls

using namespace testing;

TEST(NAME, default_controls_agree_with_default_snake_head)
{
    struct command controls;
    struct snake_head head1, head2;
    command_init(&controls);
    snake_head_init(&head1, make_qwposi(0, 0));
    snake_head_init(&head2, make_qwposi(0, 0));

    snake_step_head(&head2, &controls, 60);
    EXPECT_THAT(head1.angle, Eq(head2.angle));
}

TEST(NAME, inserting_sequential_controls_works)
{
    struct command c;
    command_init(&c);
    struct command_rb rb;
    command_rb_init(&rb);

    c.angle = 1;  command_rb_put(&rb, &c, 65534);
    c.angle = 2;  command_rb_put(&rb, &c, 65535);
    c.angle = 3;  command_rb_put(&rb, &c, 0);
    c.angle = 4;  command_rb_put(&rb, &c, 1);
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
    struct command c;
    command_init(&c);
    struct command_rb rb;
    command_rb_init(&rb);

    c.angle = 1;  command_rb_put(&rb, &c, 65534);
    c.angle = 2;  command_rb_put(&rb, &c, 65535);
    c.angle = 3;  command_rb_put(&rb, &c, 1);
    c.angle = 4;  command_rb_put(&rb, &c, 2);
    EXPECT_THAT(command_rb_count(&rb), Eq(2));
    EXPECT_THAT(command_rb_peek(&rb, 0)->angle, Eq(1));
    EXPECT_THAT(command_rb_peek(&rb, 1)->angle, Eq(2));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65534));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(0));

    command_rb_deinit(&rb);
}

TEST(NAME, take_first_matching_controls)
{
    struct command c;
    command_init(&c);
    struct command_rb rb;
    command_rb_init(&rb);

    c.angle = 1;  command_rb_put(&rb, &c, 65534);
    c.angle = 2;  command_rb_put(&rb, &c, 65535);
    c.angle = 3;  command_rb_put(&rb, &c, 0);
    c.angle = 4;  command_rb_put(&rb, &c, 1);
    c.angle = 5;  command_rb_put(&rb, &c, 2);
    EXPECT_THAT(command_rb_count(&rb), Eq(5));

    EXPECT_THAT(command_rb_find_or_predict(&rb, 65534)->angle, Eq(1));
    EXPECT_THAT(command_rb_find_or_predict(&rb, 65534)->angle, Eq(1));
    EXPECT_THAT(command_rb_count(&rb), Eq(5));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65534));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 65534)->angle, Eq(1));
    EXPECT_THAT(command_rb_count(&rb), Eq(4));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65535));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 65535)->angle, Eq(2));
    EXPECT_THAT(command_rb_count(&rb), Eq(3));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(0));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 0)->angle, Eq(3));
    EXPECT_THAT(command_rb_count(&rb), Eq(2));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(1));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 1)->angle, Eq(4));
    EXPECT_THAT(command_rb_count(&rb), Eq(1));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(2));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 2)->angle, Eq(5));
    EXPECT_THAT(command_rb_count(&rb), Eq(0));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(3));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    command_rb_deinit(&rb);
}

TEST(NAME, take_controls_in_past_uses_last_predicted)
{
    struct command c, default_controls;
    command_init(&c);
    command_init(&default_controls);
    struct command_rb rb;
    command_rb_init(&rb);

    c.angle = 1;  command_rb_put(&rb, &c, 65534);
    c.angle = 2;  command_rb_put(&rb, &c, 65535);
    c.angle = 3;  command_rb_put(&rb, &c, 0);
    c.angle = 4;  command_rb_put(&rb, &c, 1);
    c.angle = 5;  command_rb_put(&rb, &c, 2);
    EXPECT_THAT(command_rb_count(&rb), Eq(5));

    EXPECT_THAT(command_rb_find_or_predict(&rb, 65530)->angle, Eq(default_controls.angle));
    EXPECT_THAT(command_rb_find_or_predict(&rb, 65533)->angle, Eq(default_controls.angle));
    EXPECT_THAT(command_rb_count(&rb), Eq(5));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65534));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 65530)->angle, Eq(default_controls.angle));
    EXPECT_THAT(command_rb_take_or_predict(&rb, 65533)->angle, Eq(default_controls.angle));
    EXPECT_THAT(command_rb_count(&rb), Eq(5));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65534));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    // Should update last predicted controls
    command_rb_take_or_predict(&rb, 65534);
    EXPECT_THAT(command_rb_count(&rb), Eq(4));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65535));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_find_or_predict(&rb, 65530)->angle, Eq(1));
    EXPECT_THAT(command_rb_find_or_predict(&rb, 65533)->angle, Eq(1));
    EXPECT_THAT(command_rb_count(&rb), Eq(5));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65534));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 65530)->angle, Eq(1));
    EXPECT_THAT(command_rb_take_or_predict(&rb, 65533)->angle, Eq(1));
    EXPECT_THAT(command_rb_count(&rb), Eq(5));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65534));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    command_rb_deinit(&rb);
}

TEST(NAME, take_controls_in_middle)
{
    struct command c;
    command_init(&c);
    struct command_rb rb;
    command_rb_init(&rb);

    c.angle = 1;  command_rb_put(&rb, &c, 65534);
    c.angle = 2;  command_rb_put(&rb, &c, 65535);
    c.angle = 3;  command_rb_put(&rb, &c, 0);
    c.angle = 4;  command_rb_put(&rb, &c, 1);
    c.angle = 5;  command_rb_put(&rb, &c, 2);
    EXPECT_THAT(command_rb_count(&rb), Eq(5));

    EXPECT_THAT(command_rb_find_or_predict(&rb, 0)->angle, Eq(3));
    EXPECT_THAT(command_rb_count(&rb), Eq(5));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(65534));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    EXPECT_THAT(command_rb_take_or_predict(&rb, 0)->angle, Eq(3));
    EXPECT_THAT(command_rb_count(&rb), Eq(3));
    EXPECT_THAT(command_rb_frame_begin(&rb), Eq(1));
    EXPECT_THAT(command_rb_frame_end(&rb), Eq(3));

    command_rb_deinit(&rb);
}
