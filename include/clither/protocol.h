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
    MSG_JOIN_REQUEST,
    MSG_JOIN_ACCEPT,
    MSG_JOIN_DENY_BAD_PROTOCOL,
    MSG_JOIN_DENY_BAD_USERNAME,
    MSG_JOIN_DENY_SERVER_FULL,
    MSG_JOIN_ACK,
    MSG_LEAVE,

    MSG_SNAKE_METADATA,
    MSG_SNAKE_METADATA_ACK,
    MSG_SNAKE_DATA,

    MSG_FOOD_CREATE,
    MSG_FOOD_CREATE_ACK,
    MSG_FOOD_DESTROY,
    MSG_FOOD_DESTROY_ACK
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
server_join_game_accept(
        struct cs_vector* msg_queue,
        struct qpos2* spawn_pos);

void
server_join_game_deny_bad_username(
        struct cs_vector* msg_queue,
        const char* error);

void
server_join_game_deny_server_full(
        struct cs_vector* msg_queue,
        const char* error);

void
server_join_game_ack_received(struct cs_vector* msg_queue);

void
client_join_game_request(
        struct cs_vector* msg_queue,
        const char* username);

void
client_join_game_accepted(struct cs_vector* msg_queue);

void
client_join_game_ack(struct cs_vector* msg_queue);

void
client_leave(struct cs_vector* msg_queue);

#define NET_MSG_FOR_EACH(queue, var) \
    VECTOR_FOR_EACH((queue), struct net_msg*, vec_##var) \
    struct net_msg* var = *(vec_##var); {
#define NET_MSG_END_EACH \
    VECTOR_END_EACH }
#define NET_MSG_ERASE_IN_FOR_LOOP(queue, var) \
    VECTOR_ERASE_IN_FOR_LOOP(queue, struct net_msg*, vec_##var)

C_END
