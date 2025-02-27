#pragma once

#include "clither/msg.h"
#include "clither/vec.h"

VEC_DECLARE(msg_vec, struct msg*, 16)

void msg_vec_remove_type(struct msg_vec* q, enum msg_type type);
