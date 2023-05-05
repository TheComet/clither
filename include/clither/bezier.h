#pragma once

#include "clither/q.h"
#include <stdint.h>

struct bezier_handle
{
    struct qpos2 pos;
    uint8_t angle;
    uint8_t len1, len2;
};

void
bezier_handle_init(struct bezier_handle* bh);
