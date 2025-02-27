#include "clither/msg_vec.h"

VEC_DEFINE(msg_vec, struct msg*, 16)

/* ------------------------------------------------------------------------- */
static int retain_type(struct msg** msgq, void* user)
{
    if ((*msgq)->type == (enum msg_type)(intptr_t)user)
    {
        msg_free(*msgq);
        return VEC_ERASE;
    }

    return VEC_RETAIN;
}

/* ------------------------------------------------------------------------- */
void msg_vec_remove_type(struct msg_vec* msgq, enum msg_type type)
{
    msg_vec_retain(msgq, retain_type, (void*)(intptr_t)type);
}
