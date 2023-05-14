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

    MSG_CONTROLS,
    MSG_CONTROLS_ACK,

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
    int8_t resend_rate;
    int8_t resend_rate_counter;

    enum msg_type type;
    uint8_t payload_len;
    char payload[1];
};

union parsed_payload
{
    struct {
        const char* username;
        uint16_t protocol_version;
        uint16_t frame;
        uint8_t username_len;
    } join_request;

    struct {
        struct qwpos2 spawn;
        uint16_t client_frame;
        uint16_t server_frame;
        uint8_t sim_tick_rate;
        uint8_t net_tick_rate;
    } join_accept;

    struct {
        const char* error;
    } join_deny;
};

int
msg_parse_paylaod(
    union parsed_payload* pl,
    enum msg_type type,
    uint8_t payload_len,
    const char* payload);

void
msg_free(struct msg* m);

void
msg_update_frame_number(struct msg* m, uint16_t frame_number);

struct msg*
msg_join_request(uint16_t protocol_version, uint16_t frame_number, const char* username);

struct msg*
msg_join_accept(
    uint8_t sim_tick_rate,
    uint8_t net_tick_rate,
    uint16_t client_frame_number,
    uint16_t server_frame_number,
    struct qwpos2* spawn_pos);

struct msg*
msg_join_deny_bad_protocol(const char* error);

struct msg*
msg_join_deny_bad_username(const char* error);

struct msg*
msg_join_deny_server_full(const char* error);

struct msg*
msg_join_ack(void);

struct msg*
msg_leave(void);

struct msg*
msg_controls(void);

C_END
