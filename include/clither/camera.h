#pragma once

#include "clither/config.h"
#include "clither/q.h"

C_BEGIN

struct camera
{
    struct qwpos2 pos;
    qw scale;
};

void
camera_init(struct camera* c);

C_END