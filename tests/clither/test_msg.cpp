#include "gmock/gmock.h"

#include "clither/command.h"
#include "clither/msg.h"
#include "cstructures/vector.h"

#define NAME msg

using namespace testing;

bool operator==(const struct command& a, const struct command& b)
{
    return a.action == b.action && a.action == b.action && a.speed == b.speed;
}

TEST(NAME, compress_single_controls)
{
    struct command_rb rb;
    command_rb_init(&rb);

    struct command c0 = {25, 8, 0};

    command_rb_put(&rb, c0, 20);

    struct cs_vector msgs;
    vector_init(&msgs, sizeof(struct msg*));
    msg_commands(&msgs, &rb);
    struct msg* m = *(struct msg**)vector_front(&msgs);
    command_rb_clear(&rb);
    vector_deinit(&msgs);

    uint16_t first_frame, last_frame;
    ASSERT_THAT(msg_commands_unpack_into(&rb, m->payload, m->payload_len, 15, &first_frame, &last_frame), Eq(0));
    msg_free(m);

    ASSERT_THAT(command_rb_count(&rb), Eq(1));
    EXPECT_THAT(command_rb_find_or_predict(&rb, 20), Eq(c0));

    command_rb_deinit(&rb);
}

TEST(NAME, compress_multiple_controls)
{
    struct command_rb rb;
    command_rb_init(&rb);

    struct command c0 = {25, 8, 1};
    struct command c1 = {28, 20, 0};
    struct command c2 = {31, 30, 2};
    struct command c3 = {34, 40, 3};

    command_rb_put(&rb, c0, 20);
    command_rb_put(&rb, c1, 21);
    command_rb_put(&rb, c2, 22);
    command_rb_put(&rb, c3, 23);

    struct cs_vector msgs;
    vector_init(&msgs, sizeof(struct msg*));
    msg_commands(&msgs, &rb);
    struct msg* m = *(struct msg**)vector_front(&msgs);
    command_rb_clear(&rb);

    uint16_t first_frame, last_frame;
    ASSERT_THAT(msg_commands_unpack_into(&rb, m->payload, m->payload_len, 15, &first_frame, &last_frame), Eq(0));
    msg_free(m);

    ASSERT_THAT(command_rb_count(&rb), Eq(4));
    EXPECT_THAT(command_rb_find_or_predict(&rb, 20), Eq(c0));
    EXPECT_THAT(command_rb_find_or_predict(&rb, 21), Eq(c1));
    EXPECT_THAT(command_rb_find_or_predict(&rb, 22), Eq(c2));
    EXPECT_THAT(command_rb_find_or_predict(&rb, 23), Eq(c3));

    command_rb_deinit(&rb);
}
