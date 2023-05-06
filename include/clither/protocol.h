#pragma once

#include "clither/config.h"
#include <stdint.h>

C_BEGIN

struct cs_vector;
struct qpos2;
struct client;
struct server;

enum net_msg_type
{
    MSG_JOIN,
    MSG_JOIN_ACK
};

struct net_msg
{
    enum net_msg_type type;
    int8_t priority;
    int8_t priority_counter;
    uint8_t payload_len;
    char payload[1];
};

void
net_msg_queue_init(struct cs_vector* queue);

void
net_msg_queue_deinit(struct cs_vector* queue);

void
protocol_join_game_request(struct cs_vector* msg_queue, const char* username);

void
protocol_join_game_response(struct cs_vector* msg_queue, struct qpos2* spawn_pos);

#define NET_MSG_FOR_EACH(queue, var) \
    VECTOR_FOR_EACH((queue), struct net_msg*, vec_##var) \
    struct net_msg* var = *(vec_##var); {
#define NET_MSG_END_EACH \
    VECTOR_END_EACH }

C_END
