#pragma once

#include "clither/game/msg.h"
#include "clither/util/vec.h"

VEC_DECLARE(msg_vec, struct msg*, 16)

void msg_vec_remove_type(struct msg_vec* msgq, enum msg_type type);
void msg_vec_remove_snake_username(struct msg_vec* msgq, uint16_t snake_id);
void msg_vec_remove_snake_destroy(struct msg_vec* msgq, uint16_t snake_id);
void msg_vec_remove_food_create(struct msg_vec* msgq, struct qwpos pos);
void msg_vec_remove_food_destroy(struct msg_vec* msgq, struct qwpos pos);
