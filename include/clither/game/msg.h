#pragma once

#include "clither/game/cmd_queue.h"
#include "clither/game/q.h"
#include "clither/game/settings.h"
#include <stdint.h>

struct food;
struct snake;
struct snake_param;
struct msg_vec;

enum msg_type
{
    MSG_JOIN_REQUEST,
    MSG_JOIN_ACCEPT,
    MSG_JOIN_DENY_BAD_PROTOCOL,
    MSG_JOIN_DENY_BAD_USERNAME,
    MSG_JOIN_DENY_SERVER_FULL,
    MSG_LEAVE,

    MSG_VOICE,

    MSG_COMMANDS,
    MSG_FEEDBACK,

    MSG_SNAKE_USERNAME,
    MSG_SNAKE_USERNAME_ACK,
    MSG_SNAKE_COSMETIC_PARAMS,
    MSG_SNAKE_COSMETIC_PARAMS_ACK,
    MSG_SNAKE_DESTROY,
    MSG_SNAKE_DESTROY_ACK,
    MSG_SNAKE_DEATH,
    MSG_SNAKE_DEATH_ACK,
    MSG_SNAKE_HEAD,
    MSG_SNAKE_PARAM,

    MSG_BEZIER,
    MSG_KNOT,
    MSG_KNOT_ACK,

    MSG_FOOD_CREATE,
    MSG_FOOD_CREATE_ACK,
    MSG_FOOD_DESTROY,
    MSG_FOOD_DESTROY_ACK
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
#define X(name, NAME, def, min, max) uint8_t name;
        SNAKE_COSMETIC_PARAMS_LIST
#undef X
    } join_request;

    struct
    {
        struct qwpos spawn;
        uint16_t     snake_id;
        uint16_t     client_frame;
        uint16_t     server_frame;
        uint8_t      sim_tick_rate;
        uint8_t      net_tick_rate;
        uint8_t      world_inner_radius;
        uint8_t      world_ring_start;
        uint8_t      world_ring_end;
    } join_accept;

    struct
    {
        const char* error;
    } join_deny;

    struct
    {
        const void* data;
        uint16_t    snake_id;
        uint8_t     sequence_number;
        uint8_t     size;
    } voice;

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
        const char* username;
        uint16_t    snake_id;
    } snake_username;

    struct
    {
        uint16_t snake_id;
    } snake_username_ack;

    struct
    {
        uint16_t snake_id;
#define X(name, NAME, def, min, max) uint8_t name;
        SNAKE_COSMETIC_PARAMS_LIST
#undef X
    } snake_cosmetic_params;

    struct
    {
        uint16_t snake_id;
    } snake_cosmetic_params_ack;

    struct
    {
        uint16_t snake_id;
    } snake_destroy;

    struct
    {
        uint16_t snake_id;
    } snake_destroy_ack;

    struct
    {
        struct qwpos pos;
        uint32_t     food_eaten;
        uint16_t     frame_number;
        qa           angle;
        uint8_t      speed;
    } snake_head;

    struct
    {
        uint16_t snake_id;
        uint32_t food_eaten;
    } snake_param;

    struct
    {
        struct qwpos pos;
        uint16_t     snake_id;
        uint16_t     frame_number;
        int16_t      rb_read;
        int16_t      rb_write;
        qa           angle;
        uint8_t      speed;
        uint8_t      head_len_backwards;
        uint8_t      second_len_forwards;
    } bezier;

    struct
    {
        struct qwpos pos;
        uint16_t     snake_id;
        qa           angle;
        int16_t      knot_idx;
        uint8_t      len_backwards;
        uint8_t      len_forwards;
    } knot;

    struct
    {
        uint16_t snake_id;
        int16_t  knot_idx;
    } knot_ack;

    struct
    {
        struct qwpos pos;
        struct qwpos dir;
    } food_create;

    struct
    {
        struct qwpos pos;
    } food_create_ack;

    struct
    {
        struct qwpos pos;
    } food_destroy;

    struct
    {
        struct qwpos pos;
    } food_destroy_ack;
};

int msg_parse_payload(
    union parsed_payload* pl,
    enum msg_type         type,
    const uint8_t*        payload,
    uint8_t               payload_len);

void msg_free(struct msg* m);

#define msg_is_reliable(m)   ((m)->resend_period > 0)
#define msg_is_unreliable(m) ((m)->resend_period == 0)

struct msg* msg_join_request(
    uint16_t                     protocol_version,
    uint16_t                     frame_number,
    const char*                  username,
    const struct settings_snake* settings);

struct msg* msg_join_accept(
    uint16_t      client_frame,
    uint16_t      server_frame,
    uint8_t       sim_tick_rate,
    uint8_t       net_tick_rate,
    uint8_t       world_inner_radius,
    uint8_t       world_ring_start,
    uint8_t       world_ring_end,
    uint16_t      snake_id,
    struct qwpos* spawn_pos);

struct msg* msg_join_deny_bad_protocol(const char* error);
struct msg* msg_join_deny_bad_username(const char* error);
struct msg* msg_join_deny_server_full(const char* error);
struct msg* msg_leave(void);

struct msg* msg_voice(
    uint16_t snake_id, const void* data, uint8_t size, uint8_t sequence_number);

void msg_commands(struct msg_vec** msgs, const struct cmd_queue* cmdq);
int  msg_commands_unpack_into(
     struct cmd_queue* cmdq,
     const uint8_t*    payload,
     uint8_t           payload_len,
     uint16_t          frame_number,
     uint16_t*         first_frame,
     uint16_t*         last_frame);
struct msg* msg_feedback(int8_t diff, uint16_t frame_number);

struct msg* msg_snake_username(uint16_t snake_id, const char* username);
struct msg* msg_snake_username_ack(uint16_t snake_id);
struct msg*
msg_snake_cosmetic_params(uint16_t snake_id, const struct snake_param* param);
struct msg* msg_snake_cosmetic_params_ack(uint16_t snake_id);
struct msg* msg_snake_destroy(uint16_t snake_id);
struct msg* msg_snake_destroy_ack(uint16_t snake_id);
struct msg* msg_snake_death(void);
struct msg* msg_snake_death_ack(void);
struct msg* msg_snake_head(
    uint16_t     frame_number,
    struct qwpos pos,
    qa           angle,
    uint8_t      speed,
    uint32_t     food_eaten);
struct msg* msg_snake_param(uint16_t snake_id, uint32_t food_eaten);

struct msg* msg_bezier(
    uint16_t     snake_id,
    uint16_t     frame_number,
    int16_t      rb_read,
    int16_t      rb_write,
    struct qwpos head_pos,
    qa           head_angle,
    uint8_t      head_speed,
    uint8_t      head_len_backwards,
    uint8_t      second_len_forwards);
struct msg* msg_knot(
    uint16_t     snake_id,
    uint16_t     knot_idx,
    struct qwpos pos,
    qa           angle,
    uint8_t      len_backwards,
    uint8_t      len_forwards);
struct msg* msg_knot_ack(uint16_t snake_id, int16_t knot_idx);

struct msg* msg_food_create(struct qwpos pos, struct qwpos dir);
struct msg* msg_food_create_ack(struct qwpos pos);
struct msg* msg_food_destroy(struct qwpos pos);
struct msg* msg_food_destroy_ack(struct qwpos pos);
