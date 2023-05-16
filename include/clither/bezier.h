#pragma once

#include "clither/config.h"
#include "clither/q.h"
#include <stdint.h>

C_BEGIN

struct cs_vector;

struct bezier_handle
{
    struct qwpos2 pos;
    qa angle;
    uint8_t len;
};

void
bezier_handle_init(struct bezier_handle* bh, struct qwpos2 pos, uint8_t angle, uint8_t len);

qw
bezier_fit_head(
        struct bezier_handle* tail,
        struct bezier_handle* head,
        const struct cs_vector* points);

C_END
