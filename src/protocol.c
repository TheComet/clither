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
server_join_game_accept(struct cs_vector* msg_queue, struct qpos2* spawn_pos)
{
    struct net_msg* msg = msg_new((uint8_t)sizeof(*spawn_pos), MSG_JOIN_ACCEPT, 10);
    memcpy(msg->payload, spawn_pos, sizeof(*spawn_pos));
    vector_push(msg_queue, &msg);

    log_dbg("Protocol: MSG_JOIN -> %d,%d\n", spawn_pos->x, spawn_pos->y);
}

/* ------------------------------------------------------------------------- */
static void
join_deny(
        struct cs_vector* msg_queue,
        enum net_msg_type type,
        const char* error)
{
    struct net_msg* msg;
    int error_len_i32;
    uint8_t error_len;

    error_len_i32 = strlen(error);
    if (error_len_i32 > 254)
        error_len_i32 = 254;
    error_len = (uint8_t)error_len_i32;

    msg = msg_new(error_len + 1, type, 10);
    memcpy(msg->payload + 0, &error_len, 1);
    memcpy(msg->payload + 1, error, error_len);
    vector_push(msg_queue, &msg);
}

/* ------------------------------------------------------------------------- */
void
server_join_game_deny_bad_username(
        struct cs_vector* msg_queue,
        const char* error)
{
    join_deny(msg_queue, MSG_JOIN_DENY_BAD_USERNAME, error);
    log_dbg("Protocol: MSG_JOIN_DENY_BAD_USERNAME -> \"%s\"\n", error);
}

/* ------------------------------------------------------------------------- */
void
server_join_game_deny_server_full(
        struct cs_vector* msg_queue,
        const char* error)
{
    join_deny(msg_queue, MSG_JOIN_DENY_SERVER_FULL, error);
    log_dbg("Protocol: MSG_JOIN_DENY_SERVER_FULL -> \"%s\"\n", error);
}

/* ------------------------------------------------------------------------- */
void
server_join_game_ack_received(struct cs_vector* msg_queue)
{
    /*
     * Client has acknowledged our join message response, remove it from the
     * message queue
     */
    NET_MSG_FOR_EACH(msg_queue, msg)
        if (msg->type == MSG_JOIN_ACCEPT ||
            msg->type == MSG_JOIN_DENY_BAD_PROTOCOL ||
            msg->type == MSG_JOIN_DENY_BAD_USERNAME ||
            msg->type == MSG_JOIN_DENY_SERVER_FULL)
        {
            FREE(msg);
            NET_MSG_ERASE_IN_FOR_LOOP(msg_queue, msg);
            log_dbg("Protocol: Removed MSG_JOIN response from queue\n");
        }
    NET_MSG_END_EACH
}

/* ------------------------------------------------------------------------- */
void
client_join_game_request(struct cs_vector* msg_queue, const char* username)
{
    struct net_msg* msg;
    int username_len_i32;
    uint8_t username_len;

    username_len_i32 = strlen(username);
    if (username_len_i32 > 254)
        username_len_i32 = 254;
    username_len = (uint8_t)username_len_i32;

    msg = msg_new(username_len + 1, MSG_JOIN_REQUEST, 10);
    memcpy(msg->payload + 0, &username_len, 1);
    memcpy(msg->payload + 1, username, username_len);
    vector_push(msg_queue, &msg);

    log_dbg("Protocol: MSG_JOIN -> \"%s\"\n", username);
}

/* ------------------------------------------------------------------------- */
void
client_join_game_accepted(struct cs_vector* msg_queue)
{
    /*
     * Server has acknowledged our join message request, remove it from the
     * message queue
     */
    NET_MSG_FOR_EACH(msg_queue, msg)
        if (msg->type == MSG_JOIN_REQUEST)
        {
            FREE(msg);
            NET_MSG_ERASE_IN_FOR_LOOP(msg_queue, msg);
            log_dbg("Protocol: Removed MSG_JOIN_REQUEST response from queue\n");
        }
    NET_MSG_END_EACH
}

/* ------------------------------------------------------------------------- */
void
client_join_game_ack(struct cs_vector* msg_queue)
{
    struct net_msg* msg = msg_new(0, MSG_JOIN_ACK, 0);
    vector_push(msg_queue, &msg);
}

/* ------------------------------------------------------------------------- */
void
client_leave(struct cs_vector* msg_queue)
{
    struct net_msg* msg = msg_new(0, MSG_LEAVE, 0);
    vector_push(msg_queue, &msg);
}
