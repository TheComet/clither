#include "clither/net.h"
#include "clither/protocol.h"
#include "clither/q.h"
#include "cstructures/memory.h"

#include <stdlib.h>
#include <string.h>

/* Because net_msg.payload is defined as char[1] */
#define NET_MSG_SIZE(extra_bytes) \
    (offsetof(struct net_msg, payload) + (extra_bytes))

#define MALLOC_NET_MSG(extra_bytes) \
    MALLOC(NET_MSG_SIZE(extra_bytes))

/* ------------------------------------------------------------------------- */
static void
init_msg(struct net_msg* msg, enum net_msg_type type, int8_t priority)
{
    msg->type = type;
    msg->priority = priority;
    msg->priority_counter = 0;
    msg->ack = 0;
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
client_join_game_request(struct client* client, const char* username)
{
    int username_len = strlen(username);
    struct net_msg* msg = MALLOC_NET_MSG(username_len + 1);
    init_msg(msg, MSG_JOIN, 10);
    memcpy(msg->payload, username, username_len + 1);
    vector_push(&client->pending_reliable, &msg);
}

/* ------------------------------------------------------------------------- */
void
server_join_game_response(struct server* server, struct qpos2* spawn_pos)
{
    struct net_msg* msg = MALLOC_NET_MSG(sizeof(*spawn_pos));
    init_msg(msg, MSG_JOIN, 10);
    memcpy(msg->payload, spawn_pos, sizeof(*spawn_pos));
    vector_push(&server->pending_reliable, &msg);
}
