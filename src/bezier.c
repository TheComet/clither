#include "clither/bezier.h"

/* ------------------------------------------------------------------------- */
void
bezier_handle_init(struct bezier_handle* bh, struct qwpos2 pos, uint8_t angle, uint8_t len)
{
    bh->pos = pos;
    bh->angle = angle;
    bh->len = len;
}

/* ------------------------------------------------------------------------- */
qw
bezier_fit_head(
        struct bezier_handle* tail,
        struct bezier_handle* head,
        const struct cs_vector* points)
{
    return 0;
}
