#pragma once

#include "clither/config.h"
#include "clither/q.h"
#include "clither/snake_head.h"
#include <stdint.h>

C_BEGIN

struct cs_hashmap;
struct cs_rb;
struct cs_vector;
struct command_rb;
struct snake;

enum msg_type
{
    MSG_JOIN_REQUEST,
    MSG_JOIN_ACCEPT,
    MSG_JOIN_DENY_BAD_PROTOCOL,
    MSG_JOIN_DENY_BAD_USERNAME,
    MSG_JOIN_DENY_SERVER_FULL,
    MSG_LEAVE,

    MSG_COMMANDS,
    MSG_FEEDBACK,

    MSG_SNAKE_CREATE,
    MSG_SNAKE_CREATE_ACK,
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
    } command;

    struct {
        uint16_t frame_number;
        int8_t diff;
    } feedback;

    struct {
        uint16_t snake_id;
        const char* username;
    } snake_metadata;

    struct {
        struct snake_head head;
        uint16_t frame_number;
    } snake_head;

    struct {
        const uint8_t* handles_buf;
        int handle_idx_start;
        int handle_idx_end;
        uint16_t snake_id;
    } snake_bezier;
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

void
msg_commands(struct cs_vector* msgs, const struct command_rb* rb);

int
msg_commands_unpack_into(
    struct command_rb* rb,
    const uint8_t* payload,
    uint8_t payload_len,
    uint16_t frame_number,
    uint16_t* first_frame,
    uint16_t* last_frame);

struct msg*
msg_feedback(int8_t diff, uint16_t frame_number);

struct msg*
msg_snake_head(const struct snake_head* snake, uint16_t frame_number);

void
msg_snake_bezier(
    struct cs_vector* msgs,
    uint16_t snake_id,
    const struct cs_rb* bezier_handles,
    const struct cs_hashmap* bezier_handles_ackd);

struct msg*
msg_snake_bezier_ack(
    struct cs_rb* bezier_handles,
    const union parsed_payload* pp);

C_END
