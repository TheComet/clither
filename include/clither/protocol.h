#pragma once

#include <stdint.h>

struct cs_vector;

enum net_msg_type
{
    MSG_JOIN
};

struct net_msg
{
    char addr[32];  /* storage for ipv4 or ipv6 address (struct sockaddr_storage) */
    enum net_msg_type type;
    int8_t priority;
    int8_t priority_counter;
    unsigned ack : 1;
    unsigned _ : 7;
    char payload[1];
};

void
net_msg_queue_init(struct cs_vector* queue);

void
net_msg_queue_deinit(struct cs_vector* queue);

void
client_join_game_request(struct client* client, const char* username);

void
server_join_game_response(struct server* server, struct qpos2* spawn_pos);

#define NET_MSG_FOR_EACH(queue, var) \
    VECTOR_FOR_EACH((queue), struct net_msg*, var)
#define NET_MSG_END_EACH \
    VECTOR_END_EACH
