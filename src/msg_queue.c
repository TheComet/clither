#include "clither/msg_queue.h"
#include "cstructures/vector.h"

/* ------------------------------------------------------------------------- */
void
msg_queue_init(struct cs_vector* q)
{
    vector_init(q, sizeof(struct msg*));
}

/* ------------------------------------------------------------------------- */
void
msg_queue_deinit(struct cs_vector* q)
{
    MSG_FOR_EACH(q, m)
        msg_free(m);
    MSG_END_EACH
        vector_deinit(q);
}

/* ------------------------------------------------------------------------- */
void
msg_queue_remove_type(struct cs_vector* q, enum msg_type type)
{
    MSG_FOR_EACH(q, msg)
        if (msg->type == type)
        {
            msg_free(msg);
            MSG_ERASE_IN_FOR_LOOP(q, msg);
        }
    MSG_END_EACH
}
