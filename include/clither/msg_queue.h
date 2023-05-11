#pragma once

#include "clither/config.h"
#include "clither/msg.h"

C_BEGIN

struct cs_vector;

void
msg_queue_init(struct cs_vector* q);

void
msg_queue_deinit(struct cs_vector* q);

void
msg_queue_clear(struct cs_vector* q);

void
msg_queue_remove_type(struct cs_vector* q, enum msg_type type);

C_END

#define MSG_FOR_EACH(v, var) \
    VECTOR_FOR_EACH(v, struct msg*, vec_##var) \
    struct msg* var = *vec_##var; {
#define MSG_END_EACH \
    } VECTOR_END_EACH
#define MSG_ERASE_IN_FOR_LOOP(v, var) \
    VECTOR_ERASE_IN_FOR_LOOP(v, struct msg*, vec_##var)
