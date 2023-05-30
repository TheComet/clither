#pragma once

#include "clither/config.h"
#include "clither/q.h"
#include <stdint.h>

C_BEGIN

struct cs_vector;
struct cs_btree;
struct snake;

enum msg_type
{
    MSG_JOIN_REQUEST,
    MSG_JOIN_ACCEPT,
    MSG_JOIN_DENY_BAD_PROTOCOL,
    MSG_JOIN_DENY_BAD_USERNAME,
    MSG_JOIN_DENY_SERVER_FULL,
    MSG_LEAVE,

    MSG_CONTROLS,

    MSG_SNAKE_METADATA,
    MSG_SNAKE_METADATA_ACK,
    MSG_SNAKE_HEAD,
    MSG_SNAKE_BEZIER,
    MSG_SNAKE_BEZIER_ACK,

    MSG_FOOD_CREATE,
    MSG_FOOD_CREATE_ACK,
    MSG_FOOD_DESTROY,
    MSG_FOOD_DESTROY_ACK
};

struct msg
{
    uint8_t resend_rate;
    uint8_t resend_rate_counter;

    enum msg_type type;
    uint8_t payload_len;
    uint8_t payload[1];
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
        struct qwpos spawn;
        uint16_t snake_id;
        uint16_t client_frame;
        uint16_t server_frame;
        uint8_t sim_tick_rate;
        uint8_t net_tick_rate;
    } join_accept;

    struct {
        const char* error;
    } join_deny;

    struct {
        uint16_t frame_number;
    } controls;

    struct {
        struct qwpos pos;
        qa angle;
        uint16_t frame_number;
        uint8_t speed;
    } snake_head;
};

int
msg_parse_payload(
    union parsed_payload* pl,
    enum msg_type type,
    const uint8_t* payload,
    uint8_t payload_len);

void
msg_free(struct msg* m);

#define msg_is_reliable(m) ((m)->resend_rate > 0)
#define msg_is_unreliable(m) ((m)->resend_rate == 0)

void
msg_update_frame_number(struct msg* m, uint16_t frame_number);

struct msg*
msg_join_request(
    uint16_t protocol_version,
    uint16_t frame_number,
    const char* username);

struct msg*
msg_join_accept(
    uint8_t sim_tick_rate,
    uint8_t net_tick_rate,
    uint16_t client_frame_number,
    uint16_t server_frame_number,
    uint16_t snake_id,
    struct qwpos* spawn_pos);

struct msg*
msg_join_deny_bad_protocol(const char* error);

struct msg*
msg_join_deny_bad_username(const char* error);

struct msg*
msg_join_deny_server_full(const char* error);

struct msg*
msg_leave(void);

struct msg*
msg_controls(const struct cs_btree* controls);

int
msg_controls_unpack_into(
    struct snake* snake,
    uint16_t frame_number,
    const uint8_t* payload,
    uint8_t payload_len,
    uint16_t* first_frame,
    uint16_t* last_frame);

struct msg*
msg_snake_head(const struct snake* snake, uint16_t frame_number);

C_END
