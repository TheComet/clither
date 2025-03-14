#pragma once

#include "clither/q.h"
#include "clither/snake.h"
#include <stdint.h>

struct food_cluster;
struct snake;
struct msg_vec;

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

    MSG_SNAKE_USERNAME,
    MSG_SNAKE_USERNAME_ACK,
    MSG_SNAKE_BEZIER,
    MSG_SNAKE_BEZIER_ACK,
    MSG_SNAKE_DESTROY,
    MSG_SNAKE_DESTROY_ACK,
    MSG_SNAKE_HEAD,

    MSG_FOOD_GRID_PARAMS,
    MSG_FOOD_GRID_PARAMS_ACK,
    MSG_FOOD_CLUSTER_CREATE,
    MSG_FOOD_CLUSTER_CREATE_ACK,
    MSG_FOOD_CLUSTER_UPDATE,
    MSG_FOOD_CLUSTER_UPDATE_ACK
};

struct msg
{
    uint8_t       resend_period;
    uint8_t       resend_period_counter;
    uint8_t       resend_retry_counter;
    enum msg_type type;
    uint8_t       payload_len;
    uint8_t       payload[1];
};

union parsed_payload
{
    struct
    {
        const char* username;
        uint16_t    protocol_version;
        uint16_t    frame;
        uint8_t     username_len;
    } join_request;

    struct
    {
        struct qwpos spawn;
        uint16_t     snake_id;
        uint16_t     client_frame;
        uint16_t     server_frame;
        uint8_t      sim_tick_rate;
        uint8_t      net_tick_rate;
    } join_accept;

    struct
    {
        const char* error;
    } join_deny;

    struct
    {
        uint16_t frame_number;
    } command;

    struct
    {
        uint16_t frame_number;
        int8_t   diff;
    } feedback;

    struct
    {
        uint16_t    snake_id;
        const char* username;
    } snake_metadata;

    struct
    {
        struct snake_head head;
        uint16_t          frame_number;
    } snake_head;

    struct
    {
        struct qwpos pos;
        uint16_t     snake_id;
        qa           angle;
        int16_t      handle_idx;
        uint8_t      len_backwards;
        uint8_t      len_forwards;
    } snake_bezier;

    struct
    {
        uint16_t handle_idx;
    } snake_bezier_ack;
};

int msg_parse_payload(
    union parsed_payload* pl,
    enum msg_type         type,
    const uint8_t*        payload,
    uint8_t               payload_len);

void msg_free(struct msg* m);

#define msg_is_reliable(m)   ((m)->resend_period > 0)
#define msg_is_unreliable(m) ((m)->resend_period == 0)

void msg_update_frame_number(struct msg* m, uint16_t frame_number);

struct msg* msg_join_request(
    uint16_t protocol_version, uint16_t frame_number, const char* username);

struct msg* msg_join_accept(
    uint8_t       sim_tick_rate,
    uint8_t       net_tick_rate,
    uint16_t      client_frame_number,
    uint16_t      server_frame_number,
    uint16_t      snake_id,
    struct qwpos* spawn_pos);

struct msg* msg_join_deny_bad_protocol(const char* error);

struct msg* msg_join_deny_bad_username(const char* error);

struct msg* msg_join_deny_server_full(const char* error);

struct msg* msg_leave(void);

void msg_commands(struct msg_vec** msgs, const struct cmd_queue* cmdq);

int msg_commands_unpack_into(
    struct cmd_queue* cmdq,
    const uint8_t*    payload,
    uint8_t           payload_len,
    uint16_t          frame_number,
    uint16_t*         first_frame,
    uint16_t*         last_frame);

struct msg* msg_feedback(int8_t diff, uint16_t frame_number);

struct msg*
msg_snake_head(const struct snake_head* snake, uint16_t frame_number);

struct msg* msg_snake_bezier(
    uint16_t                    snake_id,
    uint16_t                    bezier_handle_idx,
    const struct bezier_handle* bezier_handle);
struct msg* msg_snake_bezier_ack(uint16_t bezier_handle_idx);

struct msg* msg_snake_destroy(uint16_t snake_id);
struct msg* msg_snake_destroy_ack(uint16_t snake_id);

struct msg*
msg_food_cluster_create(const struct food_cluster* fc, uint16_t frame_number);
