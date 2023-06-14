#include "clither/command.h"
#include "clither/msg.h"
#include "clither/log.h"
#include "clither/snake.h"
#include "clither/wrap.h"

#include "cstructures/btree.h"
#include "cstructures/memory.h"

#include <stddef.h>
#include <string.h>
#include <assert.h>

/* Because msg.payload is defined as uint8_t[1] */
#define MSG_SIZE(extra_bytes) \
    (offsetof(struct msg, payload) + (extra_bytes))

#define MALLOC_MSG(extra_bytes) \
    MALLOC(MSG_SIZE(extra_bytes))

/* ------------------------------------------------------------------------- */
static struct msg*
msg_alloc(enum msg_type type, int8_t resend_rate, int size)
{
    struct msg* msg;
    assert(size <= 255);  /* The payload length field is 1 byte */

    msg = MALLOC_MSG(size);
    msg->type = type;
    msg->resend_rate = resend_rate;
    msg->resend_rate_counter = 1;
    msg->payload_len = size;
    return msg;
}

/* ------------------------------------------------------------------------- */
void
msg_free(struct msg* m)
{
    FREE(m);
}

/* ------------------------------------------------------------------------- */
void
msg_update_frame_number(struct msg* m, uint16_t frame_number)
{
    switch (m->type)
    {
    case MSG_JOIN_REQUEST:
        log_net("msg_update_frame_number(): %x -> %x\n", (m->payload[2] << 8) | m->payload[3], frame_number);
        m->payload[2] = frame_number >> 8;
        m->payload[3] = frame_number & 0xFF;
        break;

    case MSG_JOIN_ACCEPT:
    case MSG_JOIN_DENY_BAD_PROTOCOL:
    case MSG_JOIN_DENY_BAD_USERNAME:
    case MSG_JOIN_DENY_SERVER_FULL:
    case MSG_LEAVE:
        break;

    case MSG_COMMANDS:
        break;

    case MSG_SNAKE_METADATA:
    case MSG_SNAKE_METADATA_ACK:
        break;

    case MSG_SNAKE_HEAD:
        break;

    case MSG_FOOD_CREATE:
    case MSG_FOOD_CREATE_ACK:
    case MSG_FOOD_DESTROY:
    case MSG_FOOD_DESTROY_ACK:
        break;
    }
}

/* ------------------------------------------------------------------------- */
int
msg_parse_payload(
    union parsed_payload* pp,
    enum msg_type type,
    const uint8_t* payload,
    uint8_t payload_len)
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
                log_warn("MSG_JOIN_REQUEST payload is too small\n");
                return -1;
            }

            pp->join_request.protocol_version =
                (payload[0] << 8) |
                (payload[1] << 0);
            pp->join_request.frame =
                (payload[2] << 8) |
                (payload[3] << 0);
            pp->join_request.username_len = payload[4];
            pp->join_request.username = (const char*)&payload[5];

            if (pp->join_request.username_len == 0)
            {
                log_warn("Name has zero length\n");
                return -1;
            }
            if (5+pp->join_request.username_len > payload_len)
            {
                log_warn("Name length points outside of payload\n");
                return -1;
            }
            if (payload[5+pp->join_request.username_len] != '\0')
            {
                log_warn("Name string is not properly null-terminated\n");
                return -1;
            }
        } break;

        case MSG_JOIN_ACCEPT: {
            if (payload_len < 14)
            {
                log_warn("MSG_JOIN_ACCEPT payload is too small\n");
                return -1;
            }

            pp->join_accept.sim_tick_rate = payload[0];
            pp->join_accept.net_tick_rate = payload[1];
            pp->join_accept.client_frame =
                (payload[2] << 8) |
                (payload[3] << 0);
            pp->join_accept.server_frame =
                (payload[4] << 8) |
                (payload[5] << 0);
            pp->join_accept.snake_id =
                (payload[6] << 8) |
                (payload[7] << 0);
            pp->join_accept.spawn.x =
                (payload[8] & 0x80 ? 0xFF << 24 : 0) |  /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[8] << 16) |
                (payload[9] << 8) |
                (payload[10] << 0);
            pp->join_accept.spawn.y =
                (payload[11] & 0x80 ? 0xFF << 24 : 0) |  /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[11] << 16) |
                (payload[12] << 8) |
                (payload[13] << 0);
        } break;

        case MSG_JOIN_DENY_BAD_PROTOCOL:
        case MSG_JOIN_DENY_BAD_USERNAME:
        case MSG_JOIN_DENY_SERVER_FULL: {
            uint8_t error_len;

            if (payload_len < 2)
            {
                log_warn("MSG_JOIN_DENY payload is too small\n");
                return -1;
            }

            error_len = payload[0];

            if (1+error_len > payload_len)
            {
                log_warn("Error string length points outside of payload\n");
                return -1;
            }
            if (payload[1+error_len] != '\0')
            {
                log_warn("Error string is not properly null-terminated\n");
                return -1;
            }
            pp->join_deny.error = (const char*)&payload[1];
        } break;

        case MSG_LEAVE:
            break;

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

            pp->command.frame_number =
                (payload[0] << 8) |
                (payload[1] << 0);
            log_net("MSG_COMMANDS: frame_number=%x\n", pp->command.frame_number);
        } break;

        case MSG_FEEDBACK: {
            if (payload_len < 3)
            {
                log_warn("MSG_FEEDBACK payload is too small\n");
                return -1;
            }

            pp->feedback.frame_number =
                (payload[0] << 8) |
                (payload[1] << 0);
            pp->feedback.diff =
                payload[2];
            log_net("MSG_FEEDBACK: frame=%d, diff=%d\n", pp->feedback.frame_number, pp->feedback.diff);
        } break;

        case MSG_SNAKE_METADATA:
        case MSG_SNAKE_METADATA_ACK:
            break;

        case MSG_SNAKE_HEAD: {
            if (payload_len < 11)
            {
                log_warn("MSG_SNAKE_HEAD payload is too small\n");
                return -1;
            }

            pp->snake_head.frame_number =
                (payload[0] << 8) |
                (payload[1] << 0);
            pp->snake_head.head.pos.x =
                (payload[2] & 0x80 ? 0xFF << 24 : 0) |  /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[2] << 16) |
                (payload[3] << 8) |
                (payload[4] << 0);
            pp->snake_head.head.pos.y =
                (payload[5] & 0x80 ? 0xFF << 24 : 0) |  /* Don't forget to sign extend 24-bit to 32-bit */
                (payload[5] << 16) |
                (payload[6] << 8) |
                (payload[7] << 0);
            pp->snake_head.head.angle =
                (payload[8] << 8) |
                (payload[9] << 0);
            pp->snake_head.head.speed =
                payload[10];
        } break;

        case MSG_FOOD_CREATE:
        case MSG_FOOD_CREATE_ACK:
        case MSG_FOOD_DESTROY:
        case MSG_FOOD_DESTROY_ACK:
            break;
    }

    return type;
}

/* ------------------------------------------------------------------------- */
struct msg*
msg_join_request(uint16_t protocol_version, uint16_t frame_number, const char* username)
{
    int name_len_i32 = (int)strlen(username);
    uint8_t name_len = name_len_i32 > 254 ? 254 : (uint8_t)name_len_i32;

    struct msg* m = msg_alloc(
        MSG_JOIN_REQUEST, 1,
        sizeof(protocol_version) +
        sizeof(frame_number) +
        sizeof(name_len) +
        name_len + 1  /* we need to include the null terminator */
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
struct msg*
msg_join_accept(
    uint8_t sim_tick_rate,
    uint8_t net_tick_rate,
    uint16_t client_frame,
    uint16_t server_frame,
    uint16_t snake_id,
    struct qwpos* spawn_pos)
{
    struct msg* m = msg_alloc(
        MSG_JOIN_ACCEPT, 0,
        sizeof(sim_tick_rate) +
        sizeof(net_tick_rate) +
        sizeof(client_frame) +
        sizeof(server_frame) +
        sizeof(snake_id) +
        6  /* qwpos is 2x q10.14 (24 bits) = 48 bits */
    );

    m->payload[0] = sim_tick_rate;
    m->payload[1] = net_tick_rate;

    m->payload[2] = client_frame >> 8;
    m->payload[3] = client_frame & 0xFF;

    m->payload[4] = server_frame >> 8;
    m->payload[5] = server_frame & 0xFF;

    m->payload[6] = snake_id >> 8;
    m->payload[7] = snake_id & 0xFF;

    m->payload[8] = spawn_pos->x >> 16;
    m->payload[9] = spawn_pos->x >> 8;
    m->payload[10] = spawn_pos->x & 0xFF;
    m->payload[11] = spawn_pos->y >> 16;
    m->payload[12] = spawn_pos->y >> 8;
    m->payload[13] = spawn_pos->y & 0xFF;

    return m;
}

/* ------------------------------------------------------------------------- */
static struct msg*
msg_alloc_string_payload(enum msg_type type, int8_t resend_rate, const char* str)
{
    int len_i32 = (int)strlen(str);
    uint8_t len = len_i32 > 254 ? 254 : (uint8_t)len_i32;

    struct msg* m = msg_alloc(
        type, resend_rate,
        sizeof(len) +
        len + 1  /* we need to include the null terminator */
    );
    m->payload[0] = len;
    /* we need to include the null terminator */
    memcpy(m->payload + 1, str, len + 1);
    return m;
}

/* ------------------------------------------------------------------------- */
struct msg*
msg_join_deny_bad_protocol(const char* error)
{
    return msg_alloc_string_payload(
        MSG_JOIN_DENY_BAD_PROTOCOL, 0, error);
}

/* ------------------------------------------------------------------------- */
struct msg*
msg_join_deny_bad_username(const char* error)
{
    return msg_alloc_string_payload(
        MSG_JOIN_DENY_BAD_USERNAME, 0, error);
}

/* ------------------------------------------------------------------------- */
struct msg*
msg_join_deny_server_full(const char* error)
{
    return msg_alloc_string_payload(
        MSG_JOIN_DENY_SERVER_FULL, 0, error);
}

/* ------------------------------------------------------------------------- */
struct msg*
msg_leave(void)
{
    return msg_alloc(MSG_LEAVE, 0, 0);
}

/* ------------------------------------------------------------------------- */
void
msg_commands(struct cs_vector* msgs, const struct command_rb* rb)
{
    int i, bit, byte, send_count, send_idx;
    struct command* c;
    struct msg* m;
    uint16_t first_frame_number;

    assert(command_rb_count(rb) > 0);
    first_frame_number = command_rb_frame_begin(rb);

    /*
     * The largest message payload we limit ourselves to is 255 bytes.
     * It doesn't really make sense to send more than a full second of inputs.
     */
    for (send_idx = 0; send_idx < (int)command_rb_count(rb); send_idx += 100, first_frame_number += 100)
    {
        send_count = command_rb_count(rb) - send_idx;
        if (send_count > 100)
            send_count = 100;

        log_net("Packing command for frames %d-%d\n",
            first_frame_number, (uint16_t)(first_frame_number + send_count - 1));

        /*
         * command structure: 19 bits
         * delta:
         *   - 3 bits for angle
         *   - 5 bits for speed
         *   - 4 bits for action, assuming it changes every frame (it shouldn't)
         */
        m = msg_alloc(
            MSG_COMMANDS, 0,
            sizeof(first_frame_number) +          /* frame number */
            19 + (12*send_count + 8) / 8);  /* upper bound for all command */

        m->payload[0] = first_frame_number >> 8;
        m->payload[1] = first_frame_number & 0xFF;
        m->payload[2] = (uint8_t)(send_count - 1);

        /* First command structure */
        c = command_rb_peek(rb, 0);
        m->payload[3] = c->angle;
        m->payload[4] = c->speed;
        m->payload[5] = c->action; /* 3 bits */
        bit = 3;
        byte = 5;
        log_net("  angle=%x, speed=%x, action=%x\n", c->angle, c->speed, c->action);

        /*
         * Delta compress rest of command. Note that the frame number doesn't need
         * to be included because it always increases by 1. First write all speed
         * and angle deltas. These should be guaranteed to always be less than 3
         * and 5 bits respectively (enforced by function gfx_update_command()).
         */
#define CLEAR_NEXT_BIT() do { \
        m->payload[byte] &= ~(1 << bit); \
        if (++bit >= 8) { \
            bit = 0; \
            byte++; \
        } \
    } while(0)
#define SET_NEXT_BIT() do { \
        m->payload[byte] |= (1 << bit); \
        if (++bit >= 8) { \
            bit = 0; \
            byte++; \
        } \
    } while(0)
#define SET_OR_CLEAR_NEXT_BIT(cond) do { \
        if (cond) \
            SET_NEXT_BIT(); \
        else \
            CLEAR_NEXT_BIT(); \
    } while(0)

        for (i = 1; i < send_count; i++)
        {
            struct command* prev = command_rb_peek(rb, i-1);
            struct command* next = command_rb_peek(rb, i);

            if (next->action == prev->action)
                CLEAR_NEXT_BIT();  /* Indicate nothing has changed */
            else
            {
                SET_NEXT_BIT();  /* Indicate something has changed */
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
            uint8_t da, dv;
            struct command* prev = command_rb_peek(rb, i-1);
            struct command* next = command_rb_peek(rb, i);
            int da_i32 = next->angle - prev->angle + 3;
            int dv_i32 = next->speed - prev->speed + 15;
            if (da_i32 > 128) da_i32 -= 256;
            if (da_i32 < -128) da_i32 += 256;
            da = (uint8_t)da_i32;
            dv = (uint8_t)dv_i32;

            if (da_i32 < 0 || da_i32 > 7-1)
                log_warn("Issue while compressing command: Delta angle exceeds limit! Prev: %d, Next: %d\n", prev->angle, next->angle);
            if (dv_i32 < 0 || dv_i32 > 31-1)
                log_warn("Issue while compressing command: Delta speed exceeds limit! Prev: %d, Next: %d\n", prev->speed, next->speed);

            m->payload[byte++] = ((dv << 3) & 0xF8) | (da & 0x07);
            log_net("  angle=%x, speed=%x, action=%x\n", next->angle, next->speed, next->action);
        }

        /* Adjust the actual payload length */
        assert(byte <= m->payload_len);
        m->payload_len = byte;
        vector_push(msgs, &m);
    }
}

/* ------------------------------------------------------------------------- */
int
msg_commands_unpack_into(
    struct command_rb* rb,
    const uint8_t* payload,
    uint8_t payload_len,
    uint16_t frame_number,
    uint16_t* first_frame,
    uint16_t* last_frame)
{
    int i, bit, byte, mouse_data_offset;
    uint8_t command_count;
    uint16_t first_frame_number;
    struct command command;

    first_frame_number =
        (payload[0] << 8) |
        (payload[1] & 0xFF);
    command_count =
        payload[2];

    log_net("Unpacking command from frames %d-%d, current frame=%d\n",
            first_frame_number, (uint16_t)(first_frame_number + command_count), frame_number);
    *first_frame = first_frame_number;
    *last_frame = first_frame_number + command_count;

    /* Read first command structure */
    command.angle = payload[3];
    command.speed = payload[4];
    command.action = (payload[5] & 0x07);
    if (u16_ge_wrap(first_frame_number, frame_number))
        command_rb_put(rb, command, first_frame_number);
    log_net("  angle=%x, speed=%x, action=%x\n", command.angle, command.speed, command.action);

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

        b = payload[byte] & (1<<bit);

        bit += b ? 4 : 1;
        if (bit >= 8) {
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

#define READ_NEXT_BIT_INTO(x) do { \
        x = payload[byte] & (1<<bit); \
        if (++bit >= 8) { \
            bit = 0; \
            byte++; \
        } \
    } while (0)

        READ_NEXT_BIT_INTO(b);
        if (b)
        {
            command.action = 0;
            READ_NEXT_BIT_INTO(b);
            if (b) command.action |= 0x01;
            READ_NEXT_BIT_INTO(b);
            if (b) command.action |= 0x02;
            READ_NEXT_BIT_INTO(b);
            if (b) command.action |= 0x04;
        }

        da = payload[mouse_data_offset+i] & 0x07;
        dv = (payload[mouse_data_offset+i] >> 3) & 0x1F;
        command.angle += da - 3;
        command.speed += dv - 15;

        if (u16_ge_wrap(first_frame_number + i + 1, frame_number))
            command_rb_put(rb, command, first_frame_number + i + 1);
        log_net("  angle=%x, speed=%x, action=%x\n", command.angle, command.speed, command.action);
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
struct msg*
msg_feedback(int8_t diff, uint16_t frame_number)
{
    struct msg* m = msg_alloc(
        MSG_FEEDBACK, 0,
        sizeof(diff) +
        sizeof(frame_number));

    m->payload[0] = (frame_number >> 8);
    m->payload[1] = (frame_number & 0xFF);

    m->payload[2] = diff;

    log_net("MSG_FEEDBACK: frame=%d, diff=%d\n", frame_number, diff);

    return m;
}

/* ------------------------------------------------------------------------- */
struct msg*
msg_snake_head(const struct snake_head* head, uint16_t frame_number)
{
    struct msg* m = msg_alloc(
        MSG_SNAKE_HEAD, 0,
        sizeof(frame_number) +
        6 +  /* world position (2x 24-bit qwpos) */
        2 +  /* angle (16-bit) */
        1);  /* speed (uint8_t) */

    m->payload[0] = (frame_number >> 8) & 0xFF;
    m->payload[1] = (frame_number & 0xFF);

    m->payload[2] = (head->pos.x >> 16) & 0xFF;
    m->payload[3] = (head->pos.x >> 8) & 0xFF;
    m->payload[4] = head->pos.x & 0xFF;

    m->payload[5] = (head->pos.y >> 16) & 0xFF;
    m->payload[6] = (head->pos.y >> 8) & 0xFF;
    m->payload[7] = head->pos.y & 0xFF;

    m->payload[8] = (head->angle >> 8) & 0xFF;
    m->payload[9] = head->angle & 0xFF;

    m->payload[10] = head->speed;

    return m;
}
