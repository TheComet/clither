#include "clither/tests/LogHelper.hpp"

#include "gmock/gmock.h"

extern "C" {
#include "clither/game/cmd.h"
#include "clither/game/msg.h"
#include "clither/game/msg_vec.h"
#include "clither/platform/net.h"
}

#define NAME test_msg

using namespace testing;

struct NAME : Test, LogHelper
{
};

bool operator==(const struct cmd& a, const struct cmd& b)
{
    return a.action == b.action && a.action == b.action && a.speed == b.speed;
}

TEST_F(NAME, compress_single_controls)
{
    struct cmd_queue cmdq;
    cmd_queue_init(&cmdq);

    struct cmd c0 = {25, 8, 0};

    cmd_queue_put(&cmdq, c0, 20);

    struct msg_vec* msgs;
    msg_vec_init(&msgs);
    msg_commands(&msgs, &cmdq);
    struct msg* m = *vec_first(msgs);
    cmd_queue_clear(&cmdq);
    msg_vec_deinit(msgs);

    uint16_t first_frame, last_frame;
    ASSERT_THAT(
        msg_commands_unpack_into(
            &cmdq, m->payload, m->payload_len, 15, &first_frame, &last_frame),
        Eq(0));
    msg_free(m);

    ASSERT_THAT(cmd_queue_count(&cmdq), Eq(1));
    EXPECT_THAT(cmd_queue_find_or_predict(&cmdq, 20), Eq(c0));

    cmd_queue_deinit(&cmdq);
}

TEST_F(NAME, compress_multiple_controls)
{
    struct cmd_queue rb;
    cmd_queue_init(&rb);

    struct cmd c0 = {25, 8, 1};
    struct cmd c1 = {28, 20, 0};
    struct cmd c2 = {31, 30, 2};
    struct cmd c3 = {34, 40, 3};

    cmd_queue_put(&rb, c0, 20);
    cmd_queue_put(&rb, c1, 21);
    cmd_queue_put(&rb, c2, 22);
    cmd_queue_put(&rb, c3, 23);

    struct msg_vec* msgs;
    msg_vec_init(&msgs);
    msg_commands(&msgs, &rb);
    struct msg* m = *vec_first(msgs);
    cmd_queue_clear(&rb);
    msg_vec_deinit(msgs);

    uint16_t first_frame, last_frame;
    ASSERT_THAT(
        msg_commands_unpack_into(
            &rb, m->payload, m->payload_len, 15, &first_frame, &last_frame),
        Eq(0));
    msg_free(m);

    ASSERT_THAT(cmd_queue_count(&rb), Eq(4));
    EXPECT_THAT(cmd_queue_find_or_predict(&rb, 20), Eq(c0));
    EXPECT_THAT(cmd_queue_find_or_predict(&rb, 21), Eq(c1));
    EXPECT_THAT(cmd_queue_find_or_predict(&rb, 22), Eq(c2));
    EXPECT_THAT(cmd_queue_find_or_predict(&rb, 23), Eq(c3));

    cmd_queue_deinit(&rb);
}

TEST_F(NAME, compress_multiple_controls_spanning_multiple_messages)
{
    struct cmd_queue rb;
    cmd_queue_init(&rb);

    const int MAX_CMDS = 162;
    for (int i = 0; i != MAX_CMDS; ++i)
    {
        struct cmd c;
        c.angle = 25 + i;
        c.speed = 8 + i;
        c.action = i % 3;
        cmd_queue_put(&rb, c, 20 + i);
    }

    struct msg_vec* msgs;
    msg_vec_init(&msgs);
    msg_commands(&msgs, &rb);
    cmd_queue_clear(&rb);

    struct msg** pmsg;
    vec_for_each (msgs, pmsg)
    {
        uint16_t    first_frame, last_frame;
        struct msg* m = *pmsg;
        ASSERT_THAT(
            msg_commands_unpack_into(
                &rb, m->payload, m->payload_len, 15, &first_frame, &last_frame),
            Eq(0));
        msg_free(m);
    }

    ASSERT_THAT(cmd_queue_count(&rb), Eq(MAX_CMDS));
    for (int i = 0; i != MAX_CMDS; ++i)
    {
        struct cmd c;
        c.angle = 25 + i;
        c.speed = 8 + i;
        c.action = i % 3;
        ASSERT_THAT(cmd_queue_find_or_predict(&rb, 20 + i), Eq(c));
    }

    cmd_queue_deinit(&rb);
    msg_vec_deinit(msgs);
}

TEST_F(NAME, parse_join_request_payload_too_small)
{
    // clang-format off
    uint8_t payload[] = {
        0x00, 0x00, // Protocol version
        0x00, 0x00, // Frame number
        0,          // Username length
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_REQUEST, payload, sizeof(payload) - 1),
        Eq(-1));
    ASSERT_THAT(
        log(), LogEq("[Warn ] MSG_JOIN_REQUEST: Payload size 4 too small\n"));
}

TEST_F(NAME, parse_join_request_empty_username)
{
    // clang-format off
    uint8_t payload[16] = {
        0x00, 0x00, // Protocol version
        0x00, 0x00, // Frame number
        0,          // Username length
        '\0'};
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_REQUEST, payload, 16), Eq(-2));
}

TEST_F(NAME, parse_join_request_incorrect_username_length)
{
    // clang-format off
    uint8_t payload[16] = {
        0x00, 0x00, // Protocol version
        0x00, 0x00, // Frame number
        11,         // Username length
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_REQUEST, payload, 16), Eq(-3));
}

TEST_F(NAME, parse_join_request_incorrect_username_not_null_terminated)
{
    // clang-format off
    uint8_t payload[16] = {
        0x00, 0x00, // Protocol version
        0x00, 0x00, // Frame number
        1,          // Username length
        'a', 'b'
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_REQUEST, payload, 16), Eq(-4));
}

TEST_F(NAME, parse_join_request)
{
    // clang-format off
    uint8_t payload[16] = {
        0xAA, 0xBB, // Protocol version
        0xCC, 0xDD, // Frame number
        3,         // Username length
        'a', 'b', 'c', '\0',
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07  // cosmetic params
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_REQUEST, payload, 16),
        Eq(MSG_JOIN_REQUEST));
    EXPECT_THAT(pp.join_request.protocol_version, Eq(0xAABB));
    EXPECT_THAT(pp.join_request.frame, Eq(0xCCDD));
    EXPECT_THAT(pp.join_request.username_len, Eq(3));
    EXPECT_THAT(pp.join_request.username, StrEq("abc"));
    EXPECT_THAT(pp.join_request.username[3], Eq('\0'));
    EXPECT_THAT(pp.join_request.part_spacing, Eq(0x01));
    EXPECT_THAT(pp.join_request.spine_width, Eq(0x02));
    EXPECT_THAT(pp.join_request.head_scale, Eq(0x03));
    EXPECT_THAT(pp.join_request.body_scale, Eq(0x04));
    EXPECT_THAT(pp.join_request.tail_scale, Eq(0x05));
    EXPECT_THAT(pp.join_request.girth, Eq(0x06));
    EXPECT_THAT(pp.join_request.decay, Eq(0x07));
}

TEST_F(NAME, parse_join_accept_payload_too_small)
{
    // clang-format off
    uint8_t payload[17] = {
        0xAA,             // Sim tick rate
        0xBB,             // Net tick rate
        0x12, 0x34, 0x56, // World inner radius, ring start, ring end
        0x78, 0x9A,       // Client frame
        0xBC, 0xDE,       // Server frame
        0xFE, 0xDC,       // Snake ID
        0xBA, 0x98, 0x76, // Spawn X
        0x54, 0x32, 0x10, // Spawn Y
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(msg_parse_payload(&pp, MSG_JOIN_ACCEPT, payload, 16), Eq(-1));
}

TEST_F(NAME, parse_join_accept_qw_sign_extension)
{
    // clang-format off
    uint8_t payload[17] = {
        0xAA,             // Sim tick rate
        0xBB,             // Net tick rate
        0x12, 0x34, 0x56, // World inner radius, ring start, ring end
        0x78, 0x9A,       // Client frame
        0xBC, 0xDE,       // Server frame
        0xFE, 0xDC,       // Snake ID
        0xFF, 0xFF, 0xFF, // Spawn X
        0xFF, 0xFF, 0xFF, // Spawn Y
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_ACCEPT, payload, 17),
        Eq(MSG_JOIN_ACCEPT));
    EXPECT_THAT(pp.join_accept.spawn.x, Eq(-1));
    EXPECT_THAT(pp.join_accept.spawn.y, Eq(-1));
}

TEST_F(NAME, parse_join_accept)
{
    // clang-format off
    uint8_t payload[17] = {
        0xAA,             // Sim tick rate
        0xBB,             // Net tick rate
        0x12, 0x34, 0x56, // World inner radius, ring start, ring end
        0x78, 0x9A,       // Client frame
        0xBC, 0xDE,       // Server frame
        0xFE, 0xDC,       // Snake ID
        0x0A, 0x98, 0x76, // Spawn X
        0x04, 0x32, 0x10, // Spawn Y
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_ACCEPT, payload, 17),
        Eq(MSG_JOIN_ACCEPT));
    EXPECT_THAT(pp.join_accept.sim_tick_rate, Eq(0xAA));
    EXPECT_THAT(pp.join_accept.net_tick_rate, Eq(0xBB));
    EXPECT_THAT(pp.join_accept.world_inner_radius, Eq(0x12));
    EXPECT_THAT(pp.join_accept.world_ring_start, Eq(0x34));
    EXPECT_THAT(pp.join_accept.world_ring_end, Eq(0x56));
    EXPECT_THAT(pp.join_accept.client_frame, Eq(0x789A));
    EXPECT_THAT(pp.join_accept.server_frame, Eq(0xBCDE));
    EXPECT_THAT(pp.join_accept.snake_id, Eq(0xFEDC));
    EXPECT_THAT(pp.join_accept.spawn.x, Eq(0x0A9876));
    EXPECT_THAT(pp.join_accept.spawn.y, Eq(0x043210));
}

TEST_F(NAME, parse_join_deny_payload_too_small)
{
    // clang-format off
    uint8_t payload[] = {
        4,
        'o', 'o', 'p', 's', '\0'
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_PROTOCOL, payload, 1), Eq(-1));
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_USERNAME, payload, 1), Eq(-1));
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_DENY_SERVER_FULL, payload, 1), Eq(-1));
}

TEST_F(NAME, parse_join_deny_invalid_string_length)
{
    // clang-format off
    uint8_t payload[] = {
        1,
        '\0', '\0',
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_PROTOCOL, payload, 2), Eq(-2));
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_USERNAME, payload, 2), Eq(-2));
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_DENY_SERVER_FULL, payload, 2), Eq(-2));
}

TEST_F(NAME, parse_join_deny_string_not_null_terminated)
{
    // clang-format off
    uint8_t payload[] = {
        3, 
        'o', 'o', 'p', 's'
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_PROTOCOL, payload, 5), Eq(-3));
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_USERNAME, payload, 5), Eq(-3));
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_DENY_SERVER_FULL, payload, 5), Eq(-3));
}

TEST_F(NAME, parse_join_deny)
{
    // clang-format off
    uint8_t payload[] = {
        4,
        'o', 'o', 'p', 's', '\0'
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_PROTOCOL, payload, 6),
        Eq(MSG_JOIN_DENY_BAD_PROTOCOL));
    EXPECT_THAT(pp.join_deny.error, StrEq("oops"));
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_DENY_BAD_USERNAME, payload, 6),
        Eq(MSG_JOIN_DENY_BAD_USERNAME));
    EXPECT_THAT(pp.join_deny.error, StrEq("oops"));
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_JOIN_DENY_SERVER_FULL, payload, 6),
        Eq(MSG_JOIN_DENY_SERVER_FULL));
    EXPECT_THAT(pp.join_deny.error, StrEq("oops"));
}

TEST_F(NAME, parse_bezier_payload_wrong_size)
{
    // clang-format off
    uint8_t payload[] = {
        0xAA, 0xBB, // Snake ID
        0xCC, 0xDD, // Frame number
        0x12, 0x34, // rb_read
        0x56, 0x78, // rb_write
        0x12, 0x34, 0x56, // head_pos.x
        0x65, 0x43, 0x21, // head_pos.y
        0x50, 0x51,       // head_angle
        0x34,             // speed
        0x20, 0x21,       // head_len_backwards/forwards
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_BEZIER, payload, sizeof(payload) - 1),
        Eq(-1));
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_BEZIER, payload, sizeof(payload) + 1),
        Eq(-1));
    ASSERT_THAT(
        log(),
        LogEq("[Warn ] MSG_BEZIER: Invalid payload size 18\n"
              "[Warn ] MSG_BEZIER: Invalid payload size 20\n"));
}

TEST_F(NAME, parse_bezier_negative_rb_read)
{
    // clang-format off
    uint8_t payload[] = {
        0xAA, 0xBB, // Snake ID
        0xCC, 0xDD, // Frame number
        0x80, 0x00, // rb_read
        0x56, 0x78, // rb_write
        0x12, 0x34, 0x56, // head_pos.x
        0x65, 0x43, 0x21, // head_pos.y
        0x50, 0x51,       // head_angle
        0x34,             // speed
        0x20, 0x21,       // head_len_backwards/forwards
    };
    // clang-format on

    parsed_payload pp;
    EXPECT_THAT(
        msg_parse_payload(&pp, MSG_BEZIER, payload, sizeof(payload)), Eq(-2));
    EXPECT_THAT(
        log(), LogEq("[Warn ] MSG_BEZIER: rb_read cannot be negative!\n"));
}

TEST_F(NAME, parse_bezier_negative_rb_write)
{
    // clang-format off
    uint8_t payload[] = {
        0xAA, 0xBB, // Snake ID
        0xCC, 0xDD, // Frame number
        0x12, 0x34, // rb_read
        0x80, 0x00, // rb_write
        0x12, 0x34, 0x56, // head_pos.x
        0x65, 0x43, 0x21, // head_pos.y
        0x50, 0x51,       // head_angle
        0x34,             // speed
        0x20, 0x21,       // head_len_backwards/forwards
    };
    // clang-format on

    parsed_payload pp;
    EXPECT_THAT(
        msg_parse_payload(&pp, MSG_BEZIER, payload, sizeof(payload)), Eq(-3));
    EXPECT_THAT(
        log(), LogEq("[Warn ] MSG_BEZIER: rb_write cannot be negative!\n"));
}

TEST_F(NAME, parse_bezier_qwpos_sign_extension)
{
    // clang-format off
    uint8_t payload[] = {
        0xAA, 0xBB, // Snake ID
        0xCC, 0xDD, // Frame number
        0x12, 0x34, // rb_read
        0x56, 0x78, // rb_write
        0xFF, 0xFF, 0xFF, // head_pos.x
        0xFF, 0xFF, 0xFF, // head_pos.y
        0x50, 0x51,       // head_angle
        0x34,             // speed
        0x20, 0x21,       // head_len_backwards/forwards
    };
    // clang-format on

    parsed_payload pp;
    EXPECT_THAT(
        msg_parse_payload(&pp, MSG_BEZIER, payload, sizeof(payload)),
        Eq(MSG_BEZIER))
        << log().text;
    EXPECT_THAT(pp.bezier.pos.x, Eq(-1));
    EXPECT_THAT(pp.bezier.pos.y, Eq(-1));
}

TEST_F(NAME, parse_bezier)
{
    // clang-format off
    uint8_t payload[] = {
        0xAA, 0xBB, // Snake ID
        0xCC, 0xDD, // Frame number
        0x12, 0x34, // rb_read
        0x56, 0x78, // rb_write
        0x12, 0x34, 0x56, // head_pos.x
        0x65, 0x43, 0x21, // head_pos.y
        0x50, 0x51,       // head_angle
        0x34,             // speed
        0x20, 0x21,       // head_len_backwards/forwards
    };
    // clang-format on

    parsed_payload pp;
    EXPECT_THAT(
        msg_parse_payload(&pp, MSG_BEZIER, payload, sizeof(payload)),
        Eq(MSG_BEZIER))
        << log().text;
    EXPECT_THAT(pp.bezier.snake_id, Eq(0xAABB));
    EXPECT_THAT(pp.bezier.frame_number, Eq(0xCCDD));
    EXPECT_THAT(pp.bezier.rb_read, Eq(0x1234));
    EXPECT_THAT(pp.bezier.rb_write, Eq(0x5678));
    EXPECT_THAT(pp.bezier.pos.x, Eq(0x123456));
    EXPECT_THAT(pp.bezier.pos.y, Eq(0x654321));
    EXPECT_THAT(pp.bezier.angle, Eq(0x5051));
    EXPECT_THAT(pp.bezier.speed, Eq(0x34));
    EXPECT_THAT(pp.bezier.head_len_backwards, Eq(0x20));
    EXPECT_THAT(pp.bezier.second_len_forwards, Eq(0x21));
}

TEST_F(NAME, parse_knot_payload_wrong_size)
{
    // clang-format off
    uint8_t payload[] = {
        0xAA, 0xBB,       // Snake ID
        0x45, 0x67,       // knot idx
        0x12, 0x34, 0x56, // X Position
        0x65, 0x43, 0x21, // Y Position
        0x50, 0x51,       // Angle
        0x20, 0x21,       // Length backwards/forwards
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_KNOT, payload, sizeof(payload) - 1), Eq(-1));
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_KNOT, payload, sizeof(payload) + 1), Eq(-1));
    ASSERT_THAT(
        log(),
        LogEq("[Warn ] MSG_KNOT: Invalid payload size 13\n"
              "[Warn ] MSG_KNOT: Invalid payload size 15\n"));
}

TEST_F(NAME, parse_knot_negative_knot_index)
{
    // clang-format off
    uint8_t payload[] = {
        0xAA, 0xBB,       // Snake ID
        0x80, 0x00,       // knot idx
        0x12, 0x34, 0x56, // X Position
        0x65, 0x43, 0x21, // Y Position
        0x50, 0x51,       // Angle
        0x20, 0x21,       // Length backwards/forwards
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_KNOT, payload, sizeof(payload)), Eq(-2));
}

TEST_F(NAME, parse_knot_qwpos_sign_extension)
{
    // clang-format off
    uint8_t payload[] = {
        0xAA, 0xBB, // Snake ID
        0x02, 0x00, // Handle idx
        0xFF, 0xFF, 0xFF, // X Position
        0xFF, 0xFF, 0xFF, // Y Position
        0x50, 0x51,       // Angle
        0x20, 0x21,       // Length backwards/forwards
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_KNOT, payload, sizeof(payload)),
        Eq(MSG_KNOT));
    EXPECT_THAT(pp.knot.pos.x, Eq(-1));
    EXPECT_THAT(pp.knot.pos.y, Eq(-1));
}

TEST_F(NAME, parse_knot)
{
    // clang-format off
    uint8_t payload[] = {
        0xAA, 0xBB, // Snake ID
        0x02, 0x00, // Handle idx
        0x12, 0x34, 0x56, // X Position
        0x65, 0x43, 0x21, // Y Position
        0x50, 0x51,       // Angle
        0x20, 0x21,       // Length backwards/forwards
    };
    // clang-format on

    parsed_payload pp;
    ASSERT_THAT(
        msg_parse_payload(&pp, MSG_KNOT, payload, sizeof(payload)),
        Eq(MSG_KNOT));
    EXPECT_THAT(pp.knot.snake_id, Eq(0xAABB));
    EXPECT_THAT(pp.knot.knot_idx, Eq(0x200));
    EXPECT_THAT(pp.knot.pos.x, Eq(0x123456));
    EXPECT_THAT(pp.knot.pos.y, Eq(0x654321));
    EXPECT_THAT(pp.knot.angle, Eq(0x5051));
    EXPECT_THAT(pp.knot.len_backwards, Eq(0x20));
    EXPECT_THAT(pp.knot.len_forwards, Eq(0x21));
    EXPECT_THAT(pp.knot.snake_id, Eq(0xAABB));
}
