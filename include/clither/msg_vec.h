#pragma once

#include "clither/msg.h"
#include "clither/vec.h"

VEC_DECLARE(msg_vec, struct msg*, 16)

void msg_vec_remove_type(struct msg_vec* msgq, enum msg_type type);
void msg_vec_remove_snake_username(struct msg_vec* msgq, uint16_t snake_id);
void msg_vec_remove_snake_destroy(struct msg_vec* msgq, uint16_t snake_id);
