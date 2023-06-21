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
    vector_deinit(&msgs);

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

TEST(NAME, parse_join_request_payload_too_small)
{
    uint8_t payload[5] = {
        0x00, 0x00,  // Protocol version
        0x00, 0x00,  // Frame number
        0,           // Username length
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_REQUEST, payload, 5), Eq(-1));
}

TEST(NAME, parse_join_request_empty_username)
{
    uint8_t payload[6] = {
        0x00, 0x00,  // Protocol version
        0x00, 0x00,  // Frame number
        0,           // Username length
        '\0'
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_REQUEST, payload, 6), Eq(-2));
}

TEST(NAME, parse_join_request_incorrect_username_length)
{
    uint8_t payload[7] = {
        0x00, 0x00,  // Protocol version
        0x00, 0x00,  // Frame number
        1,           // Username length
        '\0', '\0'
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_REQUEST, payload, 6), Eq(-3));
}

TEST(NAME, parse_join_request_incorrect_username_not_null_terminated)
{
    uint8_t payload[7] = {
        0x00, 0x00,  // Protocol version
        0x00, 0x00,  // Frame number
        1,           // Username length
        'a', 'b'
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_REQUEST, payload, 7), Eq(-4));
}

TEST(NAME, parse_join_request)
{
    uint8_t payload[9] = {
        0xAA, 0xBB,  // Protocol version
        0xCC, 0xDD,  // Frame number
        3,           // Username length
        'a', 'b', 'c', '\0'
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_REQUEST, payload, 9), Eq(MSG_JOIN_REQUEST));
    EXPECT_THAT(pp.join_request.protocol_version, Eq(0xAABB));
    EXPECT_THAT(pp.join_request.frame, Eq(0xCCDD));
    EXPECT_THAT(pp.join_request.username_len, Eq(3));
    EXPECT_THAT(pp.join_request.username, StrEq("abc"));
}

TEST(NAME, parse_join_accept_payload_too_small)
{
    uint8_t payload[14] = {
        0xAA,              // Sim tick rate
        0xBB,              // Net tick rate
        0x12, 0x34,        // Client frame
        0x56, 0x78,        // Server frame
        0x90, 0xA0,        // Snake ID
        0xAB, 0xCD, 0xEF,  // Spawn X
        0xFE, 0xDC, 0xBA,  // Spawn Y
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_ACCEPT, payload, 13), Eq(-1));
}

TEST(NAME, parse_join_accept_qw_sign_extension)
{
    uint8_t payload[14] = {
        0xAA,              // Sim tick rate
        0xBB,              // Net tick rate
        0x12, 0x34,        // Client frame
        0x56, 0x78,        // Server frame
        0x90, 0xA0,        // Snake ID
        0xFF, 0xFF, 0xFF,  // Spawn X
        0xFF, 0xFF, 0xFF,  // Spawn Y
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_ACCEPT, payload, 14), Eq(MSG_JOIN_ACCEPT));
    EXPECT_THAT(pp.join_accept.spawn.x, Eq(-1));
    EXPECT_THAT(pp.join_accept.spawn.y, Eq(-1));
}

TEST(NAME, parse_join_accept)
{
    uint8_t payload[14] = {
        0xAA,              // Sim tick rate
        0xBB,              // Net tick rate
        0x12, 0x34,        // Client frame
        0x56, 0x78,        // Server frame
        0x90, 0xA0,        // Snake ID
        0x0B, 0xCD, 0xEF,  // Spawn X
        0x0E, 0xDC, 0xBA,  // Spawn Y
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_ACCEPT, payload, 14), Eq(MSG_JOIN_ACCEPT));
    EXPECT_THAT(pp.join_accept.sim_tick_rate, Eq(0xAA));
    EXPECT_THAT(pp.join_accept.net_tick_rate, Eq(0xBB));
    EXPECT_THAT(pp.join_accept.client_frame, Eq(0x1234));
    EXPECT_THAT(pp.join_accept.server_frame, Eq(0x5678));
    EXPECT_THAT(pp.join_accept.snake_id, Eq(0x90A0));
    EXPECT_THAT(pp.join_accept.spawn.x, Eq(0x0BCDEF));
    EXPECT_THAT(pp.join_accept.spawn.y, Eq(0x0EDCBA));
}

TEST(NAME, parse_join_deny_payload_too_small)
{
    uint8_t payload[] = {
        4,
        'o', 'o', 'p', 's', '\0',
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_PROTOCOL, payload, 1), Eq(-1));
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_USERNAME, payload, 1), Eq(-1));
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_DENY_SERVER_FULL, payload, 1), Eq(-1));
}

TEST(NAME, parse_join_deny_invalid_string_length)
{
    uint8_t payload[] = {
        1,
        '\0', '\0',
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_PROTOCOL, payload, 2), Eq(-2));
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_USERNAME, payload, 2), Eq(-2));
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_DENY_SERVER_FULL, payload, 2), Eq(-2));
}

TEST(NAME, parse_join_deny_string_not_null_terminated)
{
    uint8_t payload[] = {
        3,
        'o', 'o', 'p', 's'
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_PROTOCOL, payload, 5), Eq(-3));
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_USERNAME, payload, 5), Eq(-3));
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_DENY_SERVER_FULL, payload, 5), Eq(-3));
}

TEST(NAME, parse_join_deny)
{
    uint8_t payload[] = {
        4,
        'o', 'o', 'p', 's', '\0'
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_PROTOCOL, payload, 6), Eq(MSG_JOIN_DENY_BAD_PROTOCOL));
    EXPECT_THAT(pp.join_deny.error, StrEq("oops"));
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_USERNAME, payload, 6), Eq(MSG_JOIN_DENY_BAD_USERNAME));
    EXPECT_THAT(pp.join_deny.error, StrEq("oops"));
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_DENY_SERVER_FULL, payload, 6), Eq(MSG_JOIN_DENY_SERVER_FULL));
    EXPECT_THAT(pp.join_deny.error, StrEq("oops"));
}

TEST(NAME, parse_snake_bezier_payload_too_small)
{
    uint8_t payload[] = {
        0xAA, 0xBB,   // Snake ID
        0x00, 0x01,   // Handle count
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_SNAKE_BEZIER, payload, 2), Eq(-1));
}

TEST(NAME, parse_snake_bezier_invalid_handle_count_1)
{
    uint8_t payload[12] = {
        0xAA, 0xBB,        // Snake ID
        0x01,              // Handle count
        0x12, 0x34, 0x56,  // X Position
        0x65, 0x43, 0x21,  // Y Position
        0x50,              // Angle
        0x20, 0x21,        // Length backwards/forwards
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_SNAKE_BEZIER, payload, 11), Eq(-2));
    ASSERT_THAT(msg_parse_payload(&pp, MSG_SNAKE_BEZIER, payload, 13), Eq(-2));
}

TEST(NAME, parse_snake_bezier_invalid_handle_count_2)
{
    uint8_t payload[22] = {
        0xAA, 0xBB,        // Snake ID
        0x02,              // Handle count

        0x12, 0x34, 0x56,  // X Position
        0x65, 0x43, 0x21,  // Y Position
        0x50,              // Angle
        0x20, 0x21,        // Length backwards/forwards

        0x23, 0x45, 0x67,  // X Position
        0x76, 0x54, 0x32,  // Y Position
        0x60,              // Angle
        0x40, 0x41,        // Length backwards/forwards
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_SNAKE_BEZIER, payload, 20), Eq(-2));
    ASSERT_THAT(msg_parse_payload(&pp, MSG_SNAKE_BEZIER, payload, 22), Eq(-2));
}

TEST(NAME, parse_snake_bezier)
{
    uint8_t payload[21] = {
        0xAA, 0xBB,        // Snake ID
        0x02,              // Handle count

        0x12, 0x34, 0x56,  // X Position
        0x65, 0x43, 0x21,  // Y Position
        0x50,              // Angle
        0x20, 0x21,        // Length backwards/forwards

        0x23, 0x45, 0x67,  // X Position
        0x76, 0x54, 0x32,  // Y Position
        0x60,              // Angle
        0x40, 0x41,        // Length backwards/forwards
    };

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_SNAKE_BEZIER, payload, 21), Eq(MSG_SNAKE_BEZIER));
    EXPECT_THAT(pp.snake_bezier.snake_id, Eq(0xAABB));
    EXPECT_THAT(pp.snake_bezier.handle_count, Eq(2));
}
