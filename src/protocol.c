#include "clither/log.h"
#include "clither/net.h"
#include "clither/protocol.h"
#include "clither/q.h"
#include "cstructures/memory.h"

#include <stddef.h>
#include <string.h>

/* Because net_msg.payload is defined as char[1] */
#define NET_MSG_SIZE(extra_bytes) \
    (offsetof(struct net_msg, payload) + (extra_bytes))

#define MALLOC_NET_MSG(extra_bytes) \
    MALLOC(NET_MSG_SIZE(extra_bytes))

/* ------------------------------------------------------------------------- */
static struct net_msg*
msg_new(uint8_t size, enum net_msg_type type, int8_t priority)
{
    struct net_msg* msg = MALLOC_NET_MSG(size);
    msg->type = type;
    msg->priority = priority;
    msg->priority_counter = 0;
    msg->payload_len = size;
    return msg;
}

/* ------------------------------------------------------------------------- */
void
net_msg_queue_init(struct cs_vector* queue)
{
    vector_init(queue, sizeof(struct net_msg*));
}

/* ------------------------------------------------------------------------- */
void
net_msg_queue_deinit(struct cs_vector* queue)
{
    NET_MSG_FOR_EACH(queue, msg)
        FREE(msg);
    NET_MSG_END_EACH
    vector_deinit(queue);
}

/* ------------------------------------------------------------------------- */
void
protocol_join_game_request(struct cs_vector* msg_queue, const char* username)
{
    struct net_msg* msg;
    int username_len_i32;
    uint8_t username_len;

    username_len_i32 = strlen(username);
    if (username_len_i32 > 254)
        username_len_i32 = 254;
    username_len = (uint8_t)username_len_i32;

    msg = msg_new(username_len + 1, MSG_JOIN, 10);
    memcpy(msg->payload + 0, &username_len, 1);
    memcpy(msg->payload + 1, username, username_len);
    vector_push(msg_queue, &msg);

    log_dbg("Protocol: MSG_JOIN -> \"%s\"\n", username);
}

/* ------------------------------------------------------------------------- */
void
protocol_join_game_response(struct cs_vector* msg_queue, struct qpos2* spawn_pos)
{
    struct net_msg* msg = msg_new((uint8_t)sizeof(*spawn_pos), MSG_JOIN, 10);
    memcpy(msg->payload, spawn_pos, sizeof(*spawn_pos));
    vector_push(msg_queue, &msg);

    log_dbg("Protocol: MSG_JOIN -> %d,%d\n", spawn_pos->x, spawn_pos->y);
}
