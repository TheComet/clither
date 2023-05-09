#pragma once

#include "clither/config.h"
#include "clither/q.h"
#include <stdint.h>

C_BEGIN

enum msg_type
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

struct msg
{
    int8_t priority;
    int8_t priority_counter;

    enum msg_type type;
    uint8_t payload_len;
    char payload[1];
};

union parsed_payload
{
    struct {
        uint16_t protocol_version;
        uint8_t username_len;
        const char* username;
    } join_request;

    struct {
        struct qpos2 spawn;
    } join_accept;

    struct {
        const char* error;
    } join_deny;
};

int
msg_parse_paylaod(union parsed_payload* pl, enum msg_type type, uint8_t payload_len, const char* payload);

struct net_msg*
msg_server_join_accept(struct qpos2* spawn_pos);

struct net_msg*
msg_server_join_deny_bad_username(const char* error);

struct net_msg*
msg_server_join_deny_server_full(const char* error);

struct net_msg*
msg_client_join_request(uint16_t protocol_version, const char* username);

struct net_msg*
msg_client_join_game_ack(void);

struct net_msg*
msg_client_leave(void);

C_END
