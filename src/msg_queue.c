#include "clither/mem.h"
#include "clither/msg_queue.h"
#include "clither/vec.h"

VEC_DEFINE(msg_queue, struct msg*, 16)

/* ------------------------------------------------------------------------- */
static int retain_type(struct msg** msg, void* user)
{
    return (*msg)->type == (enum msg_type)(intptr_t)user;
}

/* ------------------------------------------------------------------------- */
void msg_queue_remove_type(struct msg_queue* q, enum msg_type type)
{
    msg_queue_retain(q, retain_type, (void*)(intptr_t)type);
}
