#include "clither/game/cmd.h"
#include "clither/game/msg.h"
#include "clither/game/msg_vec.h"
#include "clither/game/wrap.h"
#include "clither/util/log.h"
#include "clither/util/mem.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/* Because msg.payload is defined as uint8_t[1] */
#define msg_size(extra_bytes) (offsetof(struct msg, payload) + (extra_bytes))

#define alloc_msg(extra_bytes) mem_alloc(msg_size(extra_bytes))

/* ------------------------------------------------------------------------- */
static struct msg* msg_alloc(enum msg_type type, int8_t resend_period, int size)
{
    struct msg* msg;
    CLITHER_DEBUG_ASSERT(size <= 255); /* The payload length field is 1 byte */

    if (size < 0)
        size = 255;

    msg = alloc_msg(255);
    msg->type = type;
    msg->payload_len = size;

    /* Make sure to send it immediately first */
    msg->resend_period_counter = 1;
    /* How often to resend the message (in net_tick_rate units) */
    msg->resend_period = resend_period;
    /* How many times to retry before dropping the connection */
    msg->resend_retry_counter = 50;

    return msg;
}

/* ------------------------------------------------------------------------- */
void msg_free(struct msg* m)
{
    mem_free(m);
}

/* ------------------------------------------------------------------------- */
int msg_parse_payload(
    union parsed_payload* pp,
    enum msg_type         type,
    const uint8_t*        payload,
    uint8_t               payload_len)
{
    switch (type)
    {
        case MSG_JOIN_REQUEST: {
            /*
             * 2 bytes for protocol version
             * 2 bytes for frame number
             * 1 byte for name length
             * 1 byte for string (strings are always null-terminated)
             */
            if (payload_len < 6)
            {
                log_warn(
                    "MSG_JOIN_REQUEST: Payload size %d too small\n",
                    payload_len);
                return -1;
            }

            pp->join_request.protocol_version =
                (payload[0] << 8) | (payload[1] << 0);
            pp->join_request.frame = (payload[2] << 8) | (payload[3] << 0);
            pp->join_request.username_len = payload[4];
            pp->join_request.username = (const char*)&payload[5];

            if (pp->join_request.username_len == 0)
            {
                log_warn("Name has zero length\n");
                return -2;
            }
            if (5 + pp->join_request.username_len + 1 > payload_len)
            {
                log_warn("Name length points outside of payload\n");
                return -3;
            }
            if (payload[5 + pp->join_request.username_len] != '\0')
            {
                log_warn("Name string is not properly null-terminated\n");
                return -4;
            }
            return type;
        }

        case MSG_JOIN_ACCEPT: {
            if (payload_len < 17)
            {
                log_warn("MSG_JOIN_ACCEPT payload is too small\n");
                return -1;
            }

            pp->join_accept.sim_tick_rate = payload[0];
            pp->join_accept.net_tick_rate = payload[1];

            pp->join_accept.world_inner_radius = payload[2];
            pp->join_accept.world_ring_start = payload[3];
            pp->join_accept.world_ring_end = payload[4];

            pp->join_accept.client_frame =
                (payload[5] << 8) | (payload[6] << 0);
            pp->join_accept.server_frame =
                (payload[7] << 8) | (payload[8] << 0);
            pp->join_accept.snake_id = (payload[9] << 8) | (payload[10] << 0);
            pp->join_accept.spawn.x =
                (payload[11] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[11] << 16) |
                (payload[12] << 8) | (payload[13] << 0);
            pp->join_accept.spawn.y =
                (payload[14] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[14] << 16) |
                (payload[15] << 8) | (payload[16] << 0);
            return type;
        }

        case MSG_JOIN_DENY_BAD_PROTOCOL:
        case MSG_JOIN_DENY_BAD_USERNAME:
        case MSG_JOIN_DENY_SERVER_FULL: {
            uint8_t error_len;

            /* string length + null terminator must always be present */
            if (payload_len < 2)
            {
                log_warn("MSG_JOIN_DENY payload is too small\n");
                return -1;
            }

            error_len = payload[0];
            if (1 + error_len + 1 > payload_len)
            {
                log_warn("Error string length points outside of payload\n");
                return -2;
            }

            if (payload[1 + error_len] != '\0')
            {
                log_warn("Error string is not properly null-terminated\n");
                return -3;
            }

            pp->join_deny.error = (const char*)&payload[1];
            return type;
        }

        case MSG_LEAVE: return type;

        case MSG_COMMANDS: {
            /*
             * 2 bytes for frame number
             * 1 byte for number of command structures
             * 3 bytes containing first command structure
             * 0-N bytes containing deltas of proceeding control structures
             */
            if (payload_len < 6)
            {
                log_warn("MSG_COMMANDS payload is too small\n");
                return -1;
            }

            pp->command.frame_number = (payload[0] << 8) | (payload[1] << 0);
            return type;
        }

        case MSG_FEEDBACK: {
            if (payload_len < 3)
            {
                log_warn("MSG_FEEDBACK payload is too small\n");
                return -1;
            }

            pp->feedback.frame_number = (payload[0] << 8) | (payload[1] << 0);
            pp->feedback.diff = payload[2];
            return type;
        }

        case MSG_SNAKE_USERNAME: {
            uint8_t username_len;

            if (payload_len < 3)
            {
                log_warn("MSG_SNAKE_USERNAME payload is too small\n");
                return -1;
            }

            pp->snake_username.snake_id = (payload[0] << 8) | (payload[1] << 0);

            username_len = payload[2];
            if (3 + username_len + 1 > payload_len)
            {
                log_warn("Username length points outside of payload\n");
                return -2;
            }
            if (payload[3 + username_len] != '\0')
            {
                log_warn("Username string is not properly null-terminated\n");
                return -3;
            }
            pp->snake_username.username = (const char*)&payload[3];

            return type;
        }

        case MSG_SNAKE_USERNAME_ACK: {
            if (payload_len < 2)
            {
                log_warn("MSG_SNAKE_USERNAME_ACK payload is too small\n");
                return -1;
            }

            pp->snake_username_ack.snake_id =
                (payload[0] << 8) | (payload[1] << 0);

            break;
        }

        case MSG_SNAKE_DESTROY: {
            if (payload_len < 2)
            {
                log_warn("MSG_SNAKE_DESTROY payload is too small\n");
                return -1;
            }

            pp->snake_destroy.snake_id = (payload[0] << 8) | (payload[1] << 0);

            return type;
        }

        case MSG_SNAKE_DESTROY_ACK: {
            if (payload_len < 2)
            {
                log_warn("MSG_SNAKE_DESTROY_ACK payload is too small\n");
                return -1;
            }

            pp->snake_destroy_ack.snake_id =
                (payload[0] << 8) | (payload[1] << 0);

            return type;
        }

        case MSG_SNAKE_DEATH: {
            return type;
        }

        case MSG_SNAKE_DEATH_ACK: {
            return type;
        }

        case MSG_SNAKE_HEAD: {
            if (payload_len != 14)
            {
                log_warn(
                    "MSG_SNAKE_HEAD: Invalid payload size %d\n", payload_len);
                return -1;
            }

            pp->snake_head.frame_number = (payload[0] << 8) | (payload[1] << 0);

            pp->snake_head.pos.x =
                (payload[2] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[2] << 16) |
                (payload[3] << 8) | (payload[4] << 0);

            pp->snake_head.pos.y =
                (payload[5] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[5] << 16) |
                (payload[6] << 8) | (payload[7] << 0);

            pp->snake_head.angle = (payload[8] << 8) | (payload[9] << 0);
            pp->snake_head.speed = payload[10];
            pp->snake_head.food_eaten =
                (payload[11] << 16) | (payload[12] << 8) | (payload[13] << 0);

            return type;
        }

        case MSG_SNAKE_PARAM: {
            if (payload_len != 5)
            {
                log_warn("MSG_SNAKE_PARAM payload is too small\n");
                return -1;
            }

            pp->snake_param.snake_id = (payload[0] << 8) | (payload[1] << 0);
            pp->snake_param.food_eaten =
                (payload[2] << 16) | (payload[3] << 8) | (payload[4] << 0);

            return type;
        }

        case MSG_BEZIER: {
            if (payload_len != 19)
            {
                log_warn("MSG_BEZIER: Invalid payload size %d\n", payload_len);
                return -1;
            }

            pp->bezier.snake_id = (payload[0] << 8) | (payload[1] << 0);
            pp->bezier.frame_number = (payload[2] << 8) | (payload[3] << 0);

            pp->bezier.rb_read = (payload[4] << 8) | (payload[5] << 0);
            pp->bezier.rb_write = (payload[6] << 8) | (payload[7] << 0);
            if (pp->bezier.rb_read < 0)
            {
                log_warn("MSG_BEZIER: rb_read cannot be negative!\n");
                return -2;
            }
            if (pp->bezier.rb_write < 0)
            {
                log_warn("MSG_BEZIER: rb_write cannot be negative!\n");
                return -3;
            }

            pp->bezier.pos.x =
                (payload[8] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[8] << 16) |
                (payload[9] << 8) | (payload[10] << 0);
            pp->bezier.pos.y =
                (payload[11] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[11] << 16) |
                (payload[12] << 8) | (payload[13] << 0);

            pp->bezier.angle = (payload[14] << 8) | (payload[15] << 0);
            pp->bezier.speed = payload[16];

            pp->bezier.head_len_backwards = payload[17];
            pp->bezier.second_len_forwards = payload[18];

            return type;
        }

        case MSG_KNOT: {
            if (payload_len != 14)
            {
                log_warn("MSG_KNOT: Invalid payload size %d\n", payload_len);
                return -1;
            }

            pp->knot.snake_id = (payload[0] << 8) | (payload[1] << 0);
            pp->knot.knot_idx = (payload[2] << 8) | (payload[3] << 0);
            if (pp->knot.knot_idx < 0)
                return -2;

            pp->knot.pos.x =
                (payload[4] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[4] << 16) |
                (payload[5] << 8) | (payload[6] << 0);
            pp->knot.pos.y =
                (payload[7] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[7] << 16) |
                (payload[8] << 8) | (payload[9] << 0);

            pp->knot.angle = (payload[10] << 8) | (payload[11] << 0);

            pp->knot.len_backwards = payload[12];
            pp->knot.len_forwards = payload[13];

            return type;
        }

        case MSG_KNOT_ACK: {
            if (payload_len < 2)
            {
                log_warn(
                    "MSG_SNAKE_BEZIER_ACK: Payload is too small (%d) < 2\n",
                    payload_len);
                return -1;
            }

            pp->knot_ack.snake_id = (payload[0] << 8) | (payload[1] << 0);
            pp->knot_ack.knot_idx = (payload[2] << 8) | (payload[3] << 0);
            return type;
        }

        case MSG_FOOD_CREATE: {
            qa a;
            if (payload_len < 7)
            {
                log_warn("MSG_FOOD_CREATE: Payload is too small\n");
                return -1;
            }

            pp->food_create.pos.x =
                (payload[0] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[0] << 16) |
                (payload[1] << 8) | (payload[2] << 0);
            pp->food_create.pos.y =
                (payload[3] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[3] << 16) |
                (payload[4] << 8) | (payload[5] << 0);

            a = u8_to_qa(payload[6]);
            pp->food_create.dir.x = qa_cos(a);
            pp->food_create.dir.y = qa_sin(a);

            return type;
        }

        case MSG_FOOD_CREATE_ACK: {
            if (payload_len < 2)
            {
                log_warn("MSG_FOOD_CREATE_ACK: Payload is too small\n");
                return -1;
            }

            pp->food_create_ack.pos.x =
                (payload[0] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[0] << 16) |
                (payload[1] << 8) | (payload[2] << 0);
            pp->food_create_ack.pos.y =
                (payload[3] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[3] << 16) |
                (payload[4] << 8) | (payload[5] << 0);

            return type;
        }

        case MSG_FOOD_DESTROY: {
            if (payload_len < 2)
            {
                log_warn("MSG_FOOD_DESTROY: Payload is too small\n");
                return -1;
            }

            pp->food_create_ack.pos.x =
                (payload[0] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[0] << 16) |
                (payload[1] << 8) | (payload[2] << 0);
            pp->food_create_ack.pos.y =
                (payload[3] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[3] << 16) |
                (payload[4] << 8) | (payload[5] << 0);

            return type;
        }

        case MSG_FOOD_DESTROY_ACK: {
            if (payload_len < 2)
            {
                log_warn("MSG_FOOD_DESTROY_ACK: Payload is too small\n");
                return -1;
            }

            pp->food_create_ack.pos.x =
                (payload[0] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[0] << 16) |
                (payload[1] << 8) | (payload[2] << 0);
            pp->food_create_ack.pos.y =
                (payload[3] & 0x80
                     ? 0xFF << 24
                     : 0) | /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[3] << 16) |
                (payload[4] << 8) | (payload[5] << 0);

            return type;
        }
    }

    return -1;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_join_request(
    uint16_t protocol_version, uint16_t frame_number, const char* username)
{
    int     name_len_i32 = (int)strlen(username);
    uint8_t name_len = name_len_i32 > 254 ? 254 : (uint8_t)name_len_i32;

    struct msg* m = msg_alloc(
        MSG_JOIN_REQUEST,
        1,
        2 +              /* protocol version */
            2 +          /* frame number */
            1 +          /* name length */
            name_len + 1 /* we need to include the null terminator */
    );
    m->payload[0] = protocol_version >> 8;
    m->payload[1] = protocol_version & 0xFF;
    m->payload[2] = frame_number >> 8;
    m->payload[3] = frame_number & 0xFF;
    m->payload[4] = name_len;
    /* we need to include the null terminator */
    memcpy(m->payload + 5, username, name_len + 1);
    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_join_accept(
    uint16_t      client_frame,
    uint16_t      server_frame,
    uint8_t       sim_tick_rate,
    uint8_t       net_tick_rate,
    uint8_t       world_inner_radius,
    uint8_t       world_ring_start,
    uint8_t       world_ring_end,
    uint16_t      snake_id,
    struct qwpos* spawn_pos)
{
    struct msg* m = msg_alloc(
        MSG_JOIN_ACCEPT,
        0,
        1 +     /* sim_tick_rate */
            1 + /* net_tick_rate */
            1 + /* world_inner_radius */
            1 + /* world_ring_start */
            1 + /* world_ring_end */
            2 + /* client_frame */
            2 + /* server_frame */
            2 + /* snake_id */
            6   /* spawn pos: 2x q10.14 (24 bits) = 48 bits */
    );

    m->payload[0] = sim_tick_rate;
    m->payload[1] = net_tick_rate;

    m->payload[2] = world_inner_radius;
    m->payload[3] = world_ring_start;
    m->payload[4] = world_ring_end;

    m->payload[5] = client_frame >> 8;
    m->payload[6] = client_frame & 0xFF;

    m->payload[7] = server_frame >> 8;
    m->payload[8] = server_frame & 0xFF;

    m->payload[9] = snake_id >> 8;
    m->payload[10] = snake_id & 0xFF;

    m->payload[11] = spawn_pos->x >> 16;
    m->payload[12] = spawn_pos->x >> 8;
    m->payload[13] = spawn_pos->x & 0xFF;

    m->payload[14] = spawn_pos->y >> 16;
    m->payload[15] = spawn_pos->y >> 8;
    m->payload[16] = spawn_pos->y & 0xFF;

    return m;
}

/* ------------------------------------------------------------------------- */
static struct msg* msg_alloc_string_payload(
    enum msg_type type, int8_t resend_period, int extra_bytes, const char* str)
{
    int     len_i32 = (int)strlen(str);
    uint8_t len = len_i32 > 254 ? 254 : (uint8_t)len_i32;

    struct msg* m = msg_alloc(
        type,
        resend_period,
        extra_bytes + /* Extra fields appear before the string payload */
            sizeof(len) + len + 1); /* we need to include the null terminator */

    /* we need to include the null terminator */
    m->payload[extra_bytes] = len;
    memcpy(&m->payload[extra_bytes + 1], str, len + 1);

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_join_deny_bad_protocol(const char* error)
{
    return msg_alloc_string_payload(MSG_JOIN_DENY_BAD_PROTOCOL, 0, 0, error);
}

/* ------------------------------------------------------------------------- */
struct msg* msg_join_deny_bad_username(const char* error)
{
    return msg_alloc_string_payload(MSG_JOIN_DENY_BAD_USERNAME, 0, 0, error);
}

/* ------------------------------------------------------------------------- */
struct msg* msg_join_deny_server_full(const char* error)
{
    return msg_alloc_string_payload(MSG_JOIN_DENY_SERVER_FULL, 0, 0, error);
}

/* ------------------------------------------------------------------------- */
struct msg* msg_leave(void)
{
    return msg_alloc(MSG_LEAVE, 0, 0);
}

/* ------------------------------------------------------------------------- */
void msg_commands(struct msg_vec** msgs, const struct cmd_queue* cmdq)
{
    int               i, bit, byte, send_count, send_idx;
    const struct cmd* c;
    struct msg*       m;
    uint16_t          first_frame_number;

    CLITHER_DEBUG_ASSERT(cmd_queue_count(cmdq) > 0);
    first_frame_number = cmd_queue_frame_begin(cmdq);

    /* One UDP packet can hold up to 508 bytes of payload. Assuming 12-bits per
     * command worst case, that means ~338 commands per packet. However, the
     * "payload_len" field is only 1 byte, so the actual limit of commands is
     * 255 / 12 * 8 - 8 = 162 commands.
     */
#define MAX_COMMANDS_PER_MSG 162
    for (send_idx = 0; send_idx < cmd_queue_count(cmdq);
         send_idx += MAX_COMMANDS_PER_MSG,
        first_frame_number += MAX_COMMANDS_PER_MSG)
    {
        send_count = cmd_queue_count(cmdq) - send_idx;
        if (send_count > MAX_COMMANDS_PER_MSG)
            send_count = MAX_COMMANDS_PER_MSG;

        /*
         * command structure: 19 bits
         * delta:
         *   - 3 bits for angle
         *   - 5 bits for speed
         *   - 4 bits for action, assuming it changes every frame (it shouldn't)
         */
        m = msg_alloc(
            MSG_COMMANDS,
            0,
            2 + /* frame number */
                3 +
                (12 * send_count + 8) / 8); /* upper bound for all commands */

        m->payload[0] = first_frame_number >> 8;
        m->payload[1] = first_frame_number & 0xFF;

        m->payload[2] = (uint8_t)(send_count - 1);

        /* First command structure */
        c = cmd_queue_peek(cmdq, send_idx);
        m->payload[3] = c->angle;
        m->payload[4] = c->speed;
        m->payload[5] = c->action; /* 3 bits */
        bit = 3;
        byte = 5;

        /*
         * Delta compress rest of command. Note that the frame number doesn't
         * need to be included because it always increases by 1. First write all
         * speed and angle deltas. These should be guaranteed to always be less
         * than 3 and 5 bits respectively (enforced by function
         * gfx_update_command()).
         */
#define CLEAR_NEXT_BIT()                                                       \
    do                                                                         \
    {                                                                          \
        m->payload[byte] &= ~(1 << bit);                                       \
        if (++bit >= 8)                                                        \
        {                                                                      \
            bit = 0;                                                           \
            byte++;                                                            \
        }                                                                      \
    } while (0)
#define SET_NEXT_BIT()                                                         \
    do                                                                         \
    {                                                                          \
        m->payload[byte] |= (1 << bit);                                        \
        if (++bit >= 8)                                                        \
        {                                                                      \
            bit = 0;                                                           \
            byte++;                                                            \
        }                                                                      \
    } while (0)
#define SET_OR_CLEAR_NEXT_BIT(cond)                                            \
    do                                                                         \
    {                                                                          \
        if (cond)                                                              \
            SET_NEXT_BIT();                                                    \
        else                                                                   \
            CLEAR_NEXT_BIT();                                                  \
    } while (0)

        for (i = 1; i < send_count; i++)
        {
            const struct cmd* prev = cmd_queue_peek(cmdq, send_idx + i - 1);
            const struct cmd* next = cmd_queue_peek(cmdq, send_idx + i);

            if (next->action == prev->action)
                CLEAR_NEXT_BIT(); /* Indicate nothing has changed */
            else
            {
                SET_NEXT_BIT(); /* Indicate something has changed */
                SET_OR_CLEAR_NEXT_BIT(next->action & 0x1);
                SET_OR_CLEAR_NEXT_BIT(next->action & 0x2);
                SET_OR_CLEAR_NEXT_BIT(next->action & 0x4);
            }
        }

        /* The next chunk of data neatly aligns to 8 bits, so make sure to skip
         * the current byte if it is only partially filled */
        if (bit != 0)
            byte++;

        for (i = 1; i < send_count; ++i)
        {
            uint8_t           da, dv;
            const struct cmd* prev = cmd_queue_peek(cmdq, send_idx + i - 1);
            const struct cmd* next = cmd_queue_peek(cmdq, send_idx + i);
            int               da_i32 = next->angle - prev->angle + 3;
            int               dv_i32 = next->speed - prev->speed + 15;
            if (da_i32 > 128)
                da_i32 -= 256;
            if (da_i32 < -128)
                da_i32 += 256;
            da = (uint8_t)da_i32;
            dv = (uint8_t)dv_i32;

            if (da_i32 < 0 || da_i32 > 7 - 1)
                log_warn(
                    "Issue while compressing command: Delta angle exceeds "
                    "limit! Prev: %d, Next: %d\n",
                    prev->angle,
                    next->angle);
            if (dv_i32 < 0 || dv_i32 > 31 - 1)
                log_warn(
                    "Issue while compressing command: Delta speed exceeds "
                    "limit! Prev: %d, Next: %d\n",
                    prev->speed,
                    next->speed);

            m->payload[byte++] = ((dv << 3) & 0xF8) | (da & 0x07);
        }

        /* Adjust the actual payload length */
        assert(byte <= m->payload_len);
        m->payload_len = byte;
        msg_vec_push(msgs, m);
    }
}

/* ------------------------------------------------------------------------- */
int msg_commands_unpack_into(
    struct cmd_queue* cmdq,
    const uint8_t*    payload,
    uint8_t           payload_len,
    uint16_t          frame_number,
    uint16_t*         first_frame,
    uint16_t*         last_frame)
{
    int        i, bit, byte, mouse_data_offset;
    uint8_t    command_count;
    uint16_t   first_frame_number;
    struct cmd c;

    first_frame_number = (payload[0] << 8) | (payload[1] & 0xFF);
    command_count = payload[2];

    *first_frame = first_frame_number;
    *last_frame = first_frame_number + command_count;

    /* Read first command structure */
    c.angle = payload[3];
    c.speed = payload[4];
    c.action = (payload[5] & 0x07);
    if (u16_ge_wrap(first_frame_number, frame_number))
        cmd_queue_put(cmdq, c, first_frame_number);

    if (command_count == 0)
        return 0;

    /* Determine offset to mouse data because we have to read the button and
     * mouse data together */
    /* Offsets from after reading first control structure */
    bit = 3;
    byte = 5;
    for (i = 0; i != (int)command_count; ++i)
    {
        uint8_t b;
        if (byte >= payload_len)
        {
            log_warn("Error while unpacking command: Packet too small\n");
            return -1;
        }

        b = payload[byte] & (1 << bit);

        bit += b ? 4 : 1;
        if (bit >= 8)
        {
            bit -= 8;
            byte++;
        }
    }
    /* The next chunk of data neatly aligns to 8 bits, so make sure to skip
     * the current byte if it is only partially filled */
    mouse_data_offset = bit == 0 ? byte : byte + 1;

    if (mouse_data_offset + command_count > payload_len)
    {
        log_warn("Error while unpacking command: Packet too small\n");
        return -1;
    }

    /* Offsets from after reading first control structure */
    bit = 3;
    byte = 5;
    for (i = 0; i != (int)command_count; ++i)
    {
        uint8_t b;
        uint8_t da, dv;

#define READ_NEXT_BIT_INTO(x)                                                  \
    do                                                                         \
    {                                                                          \
        x = payload[byte] & (1 << bit);                                        \
        if (++bit >= 8)                                                        \
        {                                                                      \
            bit = 0;                                                           \
            byte++;                                                            \
        }                                                                      \
    } while (0)

        READ_NEXT_BIT_INTO(b);
        if (b)
        {
            c.action = 0;
            READ_NEXT_BIT_INTO(b);
            if (b)
                c.action |= 0x01;
            READ_NEXT_BIT_INTO(b);
            if (b)
                c.action |= 0x02;
            READ_NEXT_BIT_INTO(b);
            if (b)
                c.action |= 0x04;
        }

        da = payload[mouse_data_offset + i] & 0x07;
        dv = (payload[mouse_data_offset + i] >> 3) & 0x1F;
        c.angle += da - 3;
        c.speed += dv - 15;

        if (u16_ge_wrap(first_frame_number + i + 1, frame_number))
            cmd_queue_put(cmdq, c, first_frame_number + i + 1);
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_feedback(int8_t diff, uint16_t frame_number)
{
    struct msg* m =
        msg_alloc(MSG_FEEDBACK, 0, sizeof(diff) + sizeof(frame_number));

    m->payload[0] = (frame_number >> 8);
    m->payload[1] = (frame_number & 0xFF);

    m->payload[2] = diff;

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_snake_username(uint16_t snake_id, const char* username)
{
    struct msg* m = msg_alloc_string_payload(
        MSG_SNAKE_USERNAME, 0, 2 /* snake_id */, username);
    if (m == NULL)
        return NULL;

    m->payload[0] = snake_id >> 8;
    m->payload[1] = snake_id & 0xFF;

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_snake_username_ack(uint16_t snake_id)
{
    struct msg* m = msg_alloc(MSG_SNAKE_USERNAME_ACK, 0, 2);
    if (m == NULL)
        return NULL;

    m->payload[0] = snake_id >> 8;
    m->payload[1] = snake_id & 0xFF;

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_snake_destroy(uint16_t snake_id)
{
    struct msg* m = msg_alloc(MSG_SNAKE_DESTROY, 10, 2);
    if (m == NULL)
        return NULL;

    m->payload[0] = (snake_id >> 8) & 0xFF;
    m->payload[1] = snake_id & 0xFF;

    log_dbg("MSG_SNAKE_DESTROY: snake_id=%d\n", snake_id);

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_snake_destroy_ack(uint16_t snake_id)
{
    struct msg* m = msg_alloc(MSG_SNAKE_DESTROY_ACK, 0, 2);
    if (m == NULL)
        return NULL;

    m->payload[0] = (snake_id >> 8) & 0xFF;
    m->payload[1] = snake_id & 0xFF;

    log_dbg("MSG_SNAKE_DESTROY_ACK: snake_id=%d\n", snake_id);

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_snake_death(void)
{
    log_dbg("MSG_SNAKE_DEATH\n");
    return msg_alloc(MSG_SNAKE_DEATH, 10, 0);
}

/* ------------------------------------------------------------------------- */
struct msg* msg_snake_death_ack(void)
{
    return msg_alloc(MSG_SNAKE_DEATH_ACK, 0, 0);
}

/* ------------------------------------------------------------------------- */
struct msg* msg_snake_head(
    uint16_t     frame_number,
    struct qwpos pos,
    qa           angle,
    uint8_t      speed,
    uint32_t     food_eaten)
{
    struct msg* m = msg_alloc(
        MSG_SNAKE_HEAD,
        0,
        2 +     /* frame number */
            6 + /* world position (2x 24-bit qwpos) */
            2 + /* angle (16-bit) */
            1 + /* speed (uint8_t) */
            3); /* food eaten */

    m->payload[0] = (frame_number >> 8) & 0xFF;
    m->payload[1] = (frame_number & 0xFF);

    m->payload[2] = (pos.x >> 16) & 0xFF;
    m->payload[3] = (pos.x >> 8) & 0xFF;
    m->payload[4] = pos.x & 0xFF;

    m->payload[5] = (pos.y >> 16) & 0xFF;
    m->payload[6] = (pos.y >> 8) & 0xFF;
    m->payload[7] = pos.y & 0xFF;

    m->payload[8] = (angle >> 8) & 0xFF;
    m->payload[9] = angle & 0xFF;

    m->payload[10] = speed;

    m->payload[11] = (food_eaten >> 16) & 0xFF;
    m->payload[12] = (food_eaten >> 8) & 0xFF;
    m->payload[13] = food_eaten & 0xFF;

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_snake_param(uint16_t snake_id, uint32_t food_eaten)
{
    struct msg* m = msg_alloc(
        MSG_SNAKE_PARAM,
        0,
        2 +     /* snake_id */
            3); /* food_eaten*/
    if (m == NULL)
        return NULL;

    m->payload[0] = (snake_id >> 8) & 0xFF;
    m->payload[1] = (snake_id & 0xFF);

    m->payload[2] = (food_eaten >> 16) & 0xFF;
    m->payload[3] = (food_eaten >> 8) & 0xFF;
    m->payload[4] = (food_eaten & 0xFF);

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_bezier(
    uint16_t     snake_id,
    uint16_t     frame_number,
    int16_t      rb_read,
    int16_t      rb_write,
    struct qwpos head_pos,
    qa           head_angle,
    uint8_t      head_speed,
    uint8_t      head_len_backwards,
    uint8_t      second_len_forwards)
{
    struct msg* m = msg_alloc(
        MSG_BEZIER,
        0,
        2 +     /* snake_id */
            2 + /* frame_number */
            2 + /* rb_read */
            2 + /* rb_write */
            6 + /* World position (2x 24-bit qwpos) */
            2 + /* Angle */
            1 + /* Speed */
            1 + /* len_backwards of head */
            1); /* len_forwards of previous knot */
    if (m == NULL)
        return NULL;

    m->payload[0] = snake_id >> 8;
    m->payload[1] = snake_id & 0xFF;

    m->payload[2] = frame_number >> 8;
    m->payload[3] = frame_number & 0xFF;

    m->payload[4] = rb_read >> 8;
    m->payload[5] = rb_read & 0xFF;

    m->payload[6] = rb_write >> 8;
    m->payload[7] = rb_write & 0xFF;

    m->payload[8] = (head_pos.x >> 16) & 0xFF;
    m->payload[9] = (head_pos.x >> 8) & 0xFF;
    m->payload[10] = head_pos.x & 0xFF;

    m->payload[11] = (head_pos.y >> 16) & 0xFF;
    m->payload[12] = (head_pos.y >> 8) & 0xFF;
    m->payload[13] = head_pos.y & 0xFF;

    m->payload[14] = (head_angle >> 8) & 0xFF;
    m->payload[15] = head_angle & 0xFF;

    m->payload[16] = head_speed;

    m->payload[17] = head_len_backwards;
    m->payload[18] = second_len_forwards;

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_knot(
    uint16_t     snake_id,
    uint16_t     knot_idx,
    struct qwpos pos,
    qa           angle,
    uint8_t      len_backwards,
    uint8_t      len_forwards)
{
    struct msg* m = msg_alloc(
        MSG_KNOT,
        0,
        2 +     /* snake_id */
            2 + /* knot_idx */
            6 + /* World position (2x 24-bit qwpos) */
            2 + /* Angle */
            1 + /* Length forwards */
            1); /* Length backwards */
    if (m == NULL)
        return NULL;

    m->payload[0] = snake_id >> 8;
    m->payload[1] = snake_id & 0xFF;

    m->payload[2] = knot_idx >> 8;
    m->payload[3] = knot_idx & 0xFF;

    m->payload[4] = (pos.x >> 16) & 0xFF;
    m->payload[5] = (pos.x >> 8) & 0xFF;
    m->payload[6] = pos.x & 0xFF;

    m->payload[7] = (pos.y >> 16) & 0xFF;
    m->payload[8] = (pos.y >> 8) & 0xFF;
    m->payload[9] = pos.y & 0xFF;

    m->payload[10] = (angle >> 8) & 0xFF;
    m->payload[11] = angle & 0xFF;

    m->payload[12] = len_backwards;
    m->payload[13] = len_forwards;

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_knot_ack(uint16_t snake_id, int16_t knot_idx)
{
    struct msg* m = msg_alloc(
        MSG_KNOT_ACK,
        0,
        2 +     /* snake_id */
            2); /* knot_idx */
    if (m == NULL)
        return NULL;

    m->payload[0] = (snake_id >> 8) & 0xFF;
    m->payload[1] = snake_id & 0xFF;

    m->payload[2] = (knot_idx >> 8) & 0xFF;
    m->payload[3] = knot_idx & 0xFF;

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_food_create(struct qwpos pos, struct qwpos dir)
{
    qa          a;
    struct msg* m = msg_alloc(
        MSG_FOOD_CREATE,
        10,
        6 +     /* world position (2x 24-bit qwpos) */
            1); /* angle */
    if (m == NULL)
        return NULL;

    m->payload[0] = (pos.x >> 16) & 0xFF;
    m->payload[1] = (pos.x >> 8) & 0xFF;
    m->payload[2] = pos.x & 0xFF;

    m->payload[3] = (pos.y >> 16) & 0xFF;
    m->payload[4] = (pos.y >> 8) & 0xFF;
    m->payload[5] = pos.y & 0xFF;

    a = make_qa(atan2(qw_to_float(dir.y), qw_to_float(dir.x)));
    m->payload[6] = qa_to_u8(a);

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_food_create_ack(struct qwpos pos)
{
    struct msg* m = msg_alloc(
        MSG_FOOD_CREATE_ACK, 0, 6); /* world position (2x 24-bit qwpos) */
    if (m == NULL)
        return NULL;

    m->payload[0] = (pos.x >> 16) & 0xFF;
    m->payload[1] = (pos.x >> 8) & 0xFF;
    m->payload[2] = pos.x & 0xFF;

    m->payload[3] = (pos.y >> 16) & 0xFF;
    m->payload[4] = (pos.y >> 8) & 0xFF;
    m->payload[5] = pos.y & 0xFF;

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_food_destroy(struct qwpos pos)
{
    struct msg* m = msg_alloc(
        MSG_FOOD_DESTROY, 10, 6); /* world position (2x 24-bit qwpos) */
    if (m == NULL)
        return NULL;

    m->payload[0] = (pos.x >> 16) & 0xFF;
    m->payload[1] = (pos.x >> 8) & 0xFF;
    m->payload[2] = pos.x & 0xFF;

    m->payload[3] = (pos.y >> 16) & 0xFF;
    m->payload[4] = (pos.y >> 8) & 0xFF;
    m->payload[5] = pos.y & 0xFF;

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg* msg_food_destroy_ack(struct qwpos pos)
{
    struct msg* m = msg_alloc(
        MSG_FOOD_DESTROY_ACK, 0, 6); /* world position (2x 24-bit qwpos) */
    if (m == NULL)
        return NULL;

    m->payload[0] = (pos.x >> 16) & 0xFF;
    m->payload[1] = (pos.x >> 8) & 0xFF;
    m->payload[2] = pos.x & 0xFF;

    m->payload[3] = (pos.y >> 16) & 0xFF;
    m->payload[4] = (pos.y >> 8) & 0xFF;
    m->payload[5] = pos.y & 0xFF;

    return m;
}
