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
static int retain_food(enum msg_type msg_type, struct msg** pmsg, void* user)
{
    int                  result;
    union parsed_payload pp;
    struct qwpos         create_pos;
    struct qwpos*        pos = (struct qwpos*)user;
    struct msg*          msg = *pmsg;

    if (msg->type != msg_type)
        return VEC_RETAIN;

    /* Parse the first food position from the message. This position is used to
     * identify all food pieces in the message. */
    result = msg_parse_payload(&pp, msg_type, msg->payload, msg->payload_len);
    CLITHER_DEBUG_ASSERT(result == msg_type);
    result = msg_food_unpack_next(
        msg->payload, msg->payload_len, &create_pos, &pp.food_create.state);
    CLITHER_DEBUG_ASSERT(result == 1);
    (void)result;

    if (create_pos.x == pos->x && create_pos.y == pos->y)
    {
        msg_free(msg);
        return VEC_ERASE;
    }

    return VEC_RETAIN;
}
static int retain_food_create(struct msg** pmsg, void* user)
{
    return retain_food(MSG_FOOD_CREATE, pmsg, user);
}
static int retain_food_destroy(struct msg** pmsg, void* user)
{
    return retain_food(MSG_FOOD_DESTROY, pmsg, user);
}
void msg_vec_remove_food_create(struct msg_vec* msgq, struct qwpos pos)
{
    msg_vec_retain(msgq, retain_food_create, &pos);
}
void msg_vec_remove_food_destroy(struct msg_vec* msgq, struct qwpos pos)
{
    msg_vec_retain(msgq, retain_food_destroy, &pos);
}
