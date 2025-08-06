#include "clither/game/msg_vec.h"

VEC_DEFINE(msg_vec, struct msg*, 16)

/* ------------------------------------------------------------------------- */
static int retain_type(struct msg** pmsg, void* user)
{
    struct msg* msg = *pmsg;
    if (msg->type == (enum msg_type)(intptr_t)user)
    {
        msg_free(msg);
        return VEC_ERASE;
    }

    return VEC_RETAIN;
}
void msg_vec_remove_type(struct msg_vec* msgq, enum msg_type type)
{
    msg_vec_retain(msgq, retain_type, (void*)(intptr_t)type);
}

/* ------------------------------------------------------------------------- */
static int retain_snake_username(struct msg** pmsg, void* user)
{
    int                  parse_result;
    union parsed_payload pp;
    uint16_t             snake_id = *(uint16_t*)user;
    struct msg*          msg = *pmsg;

    if (msg->type != MSG_SNAKE_USERNAME)
        return VEC_RETAIN;

    parse_result = msg_parse_payload(
        &pp, MSG_SNAKE_USERNAME, msg->payload, msg->payload_len);
    CLITHER_DEBUG_ASSERT(parse_result == MSG_SNAKE_USERNAME);
    (void)parse_result;
    if (pp.snake_username.snake_id == snake_id)
    {
        msg_free(msg);
        return VEC_ERASE;
    }

    return VEC_RETAIN;
}
void msg_vec_remove_snake_username(struct msg_vec* msgq, uint16_t snake_id)
{
    msg_vec_retain(msgq, retain_snake_username, &snake_id);
}

/* ------------------------------------------------------------------------- */
static int retain_snake_cosmetic_params(struct msg** pmsg, void* user)
{
    int                  parse_result;
    union parsed_payload pp;
    uint16_t             snake_id = *(uint16_t*)user;
    struct msg*          msg = *pmsg;

    if (msg->type != MSG_SNAKE_COSMETIC_PARAMS)
        return VEC_RETAIN;

    parse_result = msg_parse_payload(
        &pp, MSG_SNAKE_COSMETIC_PARAMS, msg->payload, msg->payload_len);
    CLITHER_DEBUG_ASSERT(parse_result == MSG_SNAKE_COSMETIC_PARAMS);
    (void)parse_result;
    if (pp.snake_cosmetic_params.snake_id == snake_id)
    {
        log_dbg("Removing snake cosmetic params for snake_id=%d\n", snake_id);
        msg_free(msg);
        return VEC_ERASE;
    }

    return VEC_RETAIN;
}
void msg_vec_remove_snake_cosmetic_params(
    struct msg_vec* msgq, uint16_t snake_id)
{
    msg_vec_retain(msgq, retain_snake_cosmetic_params, &snake_id);
}

/* ------------------------------------------------------------------------- */
static int retain_snake_destroy(struct msg** pmsg, void* user)
{
    int                  parse_result;
    union parsed_payload pp;
    uint16_t             snake_id = *(uint16_t*)user;
    struct msg*          msg = *pmsg;

    if (msg->type != MSG_SNAKE_DESTROY)
        return VEC_RETAIN;

    parse_result = msg_parse_payload(
        &pp, MSG_SNAKE_DESTROY, msg->payload, msg->payload_len);
    CLITHER_DEBUG_ASSERT(parse_result == MSG_SNAKE_DESTROY);
    (void)parse_result;
    if (pp.snake_destroy.snake_id == snake_id)
    {
        msg_free(msg);
        return VEC_ERASE;
    }

    return VEC_RETAIN;
}
void msg_vec_remove_snake_destroy(struct msg_vec* msgq, uint16_t snake_id)
{
    msg_vec_retain(msgq, retain_snake_destroy, &snake_id);
}

/* ------------------------------------------------------------------------- */
static int retain_food_create(struct msg** pmsg, void* user)
{
    int                  parse_result;
    union parsed_payload pp;
    struct qwpos*        pos = (struct qwpos*)user;
    struct msg*          msg = *pmsg;

    if (msg->type != MSG_FOOD_CREATE)
        return VEC_RETAIN;

    parse_result =
        msg_parse_payload(&pp, MSG_FOOD_CREATE, msg->payload, msg->payload_len);
    CLITHER_DEBUG_ASSERT(parse_result == MSG_FOOD_CREATE);
    (void)parse_result;
    if (pp.food_create.pos.x == pos->x && pp.food_create.pos.y == pos->y)
    {
        msg_free(msg);
        return VEC_ERASE;
    }

    return VEC_RETAIN;
}
void msg_vec_remove_food_create(struct msg_vec* msgq, struct qwpos pos)
{
    msg_vec_retain(msgq, retain_food_create, &pos);
}

/* ------------------------------------------------------------------------- */
static int retain_food_destroy(struct msg** pmsg, void* user)
{
    int                  parse_result;
    union parsed_payload pp;
    struct qwpos*        pos = (struct qwpos*)user;
    struct msg*          msg = *pmsg;

    if (msg->type != MSG_FOOD_DESTROY)
        return VEC_RETAIN;

    parse_result = msg_parse_payload(
        &pp, MSG_FOOD_DESTROY, msg->payload, msg->payload_len);
    CLITHER_DEBUG_ASSERT(parse_result == MSG_FOOD_DESTROY);
    (void)parse_result;
    if (pp.food_destroy.pos.x == pos->x && pp.food_destroy.pos.y == pos->y)
    {
        msg_free(msg);
        return VEC_ERASE;
    }

    return VEC_RETAIN;
}
void msg_vec_remove_food_destroy(struct msg_vec* msgq, struct qwpos pos)
{
    msg_vec_retain(msgq, retain_food_destroy, &pos);
}
