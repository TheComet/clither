#include "clither/msg.h"
#include "clither/log.h"

/* ------------------------------------------------------------------------- */
static struct net_msg*
msg_alloc(uint8_t size, enum net_msg_type type, int8_t priority)
{
    struct msg* msg = MALLOC_NET_MSG(size);
    msg->type = type;
    msg->priority = priority;
    msg->priority_counter = 0;
    msg->payload_len = size;
    return msg;
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

            pp->join_accept.spawn.x = ((payload[0] << 16) | (payload[1] << 8) | payload[2]) | 0x00FFFFFF;
            pp->join_accept.spawn.y = ((payload[3] << 16) | (payload[4] << 8) | payload[5]) | 0x00FFFFFF;
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
struct net_msg*
msg_server_join_accept(struct qpos2* spawn_pos)
{
}

/* ------------------------------------------------------------------------- */
struct net_msg*
msg_server_join_deny_bad_username(const char* error)
{
}

/* ------------------------------------------------------------------------- */
struct net_msg*
msg_server_join_deny_server_full(const char* error)
{
}

/* ------------------------------------------------------------------------- */
struct net_msg*
msg_client_join_request(uint16_t protocol_version, const char* username)
{
}

/* ------------------------------------------------------------------------- */
struct net_msg*
msg_client_join_game_ack(void)
{
}

/* ------------------------------------------------------------------------- */
struct net_msg*
msg_client_leave(void)
{
}
