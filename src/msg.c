#include "clither/msg.h"
#include "clither/log.h"
#include "cstructures/memory.h"
#include <stddef.h>

/* Because msg.payload is defined as char[1] */
#define MSG_SIZE(extra_bytes) \
    (offsetof(struct msg, payload) + (extra_bytes))

#define MALLOC_MSG(extra_bytes) \
    MALLOC(MSG_SIZE(extra_bytes))

/* ------------------------------------------------------------------------- */
static struct msg*
msg_alloc(enum msg_type type, int8_t resend_rate, uint8_t size)
{
    struct msg* msg = MALLOC_MSG(size);
    msg->type = type;
    msg->resend_rate = resend_rate;
    msg->resend_rate_counter = 0;
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
int
msg_parse_paylaod(
    union parsed_payload* pp,
    enum msg_type type,
    uint8_t payload_len,
    const char* payload)
{
    switch (type)
    {
        case MSG_JOIN_REQUEST: {
            uint8_t major, minor, name_len;

            if (payload_len < 4)
            {
                log_warn("MSG_JOIN_REQUEST payload is too small\n");
                return -1;
            }

            major = payload[0];
            minor = payload[1];
            name_len = payload[2];

            if (name_len == 0)
            {
                log_warn("Name has zero length\n");
                return -1;
            }
            if (3+name_len > payload_len)
            {
                log_warn("Name length points outside of payload\n");
                return -1;
            }
            if (payload[3+name_len] != '\0')
            {
                log_warn("Name string is not properly null-terminated\n");
                return -1;
            }

            pp->join_request.protocol_version = (major << 8) | minor;
            pp->join_request.username_len = name_len;
            pp->join_request.username = &payload[3];
        } break;

        case MSG_JOIN_ACCEPT: {
            if (payload_len < 6)
            {
                log_warn("MSG_JOIN_ACCEPT payload is too small\n");
                return -1;
            }

            pp->join_accept.spawn.x =
                (payload[0] << 16) |
                (payload[1] << 8) |
                (payload[2] << 0);
            pp->join_accept.spawn.y =
                (payload[3] << 16) |
                (payload[4] << 8) |
                (payload[5] << 0);
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

            pp->join_deny.error = &payload[1];
        } break;
    }

    return type;
}

/* ------------------------------------------------------------------------- */
struct msg*
msg_join_request(uint16_t protocol_version, const char* username)
{
    int name_len_i32 = strlen(username);
    uint8_t name_len = name_len_i32 > 254 ? 254 : (uint8_t)name_len_i32;

    struct msg* m = msg_alloc(
        MSG_JOIN_REQUEST, 10,
        sizeof(protocol_version) +
        sizeof(name_len) +
        name_len + 1  /* we need to include the null terminator */
    );
    m->payload[0] = protocol_version >> 8;
    m->payload[1] = protocol_version & 0xFF;
    m->payload[2] = name_len;
    /* we need to include the null terminator */
    memcpy(m->payload + 3, username, name_len + 1);
    return m;
}

/* ------------------------------------------------------------------------- */
struct msg*
msg_join_accept(struct qpos2* spawn_pos)
{
    struct msg* m = msg_alloc(
        MSG_JOIN_ACCEPT, 10,
        6  /* qpos2 is 2x q19.5 (24 bits) = 48 bits */
    );
    m->payload[0] = (spawn_pos->x >> 16) & 0xFF;
    m->payload[1] = (spawn_pos->x >> 8) & 0xFF;
    m->payload[2] = spawn_pos->x & 0xFF;
    m->payload[3] = (spawn_pos->y >> 16) & 0xFF;
    m->payload[4] = (spawn_pos->y >> 8) & 0xFF;
    m->payload[5] = spawn_pos->y & 0xFF;
    return m;
}

/* ------------------------------------------------------------------------- */
static struct msg*
msg_alloc_string_payload(enum msg_type type, int8_t resend_rate, const char* str)
{
    int len_i32 = strlen(str);
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
        MSG_JOIN_DENY_BAD_PROTOCOL, 10, error);
}

/* ------------------------------------------------------------------------- */
struct msg*
 msg_join_deny_bad_username(const char* error)
{
    return msg_alloc_string_payload(
        MSG_JOIN_DENY_BAD_USERNAME, 10, error);
}

/* ------------------------------------------------------------------------- */
struct msg*
msg_join_deny_server_full(const char* error)
{
    return msg_alloc_string_payload(
        MSG_JOIN_DENY_SERVER_FULL, 10, error);
}

/* ------------------------------------------------------------------------- */
struct msg*
msg_join_ack(void)
{
    return msg_alloc(MSG_JOIN_ACK, 10, 0);
}

/* ------------------------------------------------------------------------- */
struct msg*
msg_leave(void)
{
    return msg_alloc(MSG_LEAVE, 10, 0);
}
