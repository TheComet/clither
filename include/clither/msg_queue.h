#pragma once

#include "clither/msg.h"

VEC_DECLARE(msg_queue, struct msg*, 16)

void msg_queue_remove_type(struct msg_queue* q, enum msg_type type);
