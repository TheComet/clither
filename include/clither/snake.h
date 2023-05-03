#pragma once

#include "clither/config.h"
#include "clither/q.h"
#include "cstructures/vector.h"

C_BEGIN

struct qpos2
{
    q x;
    q y;
};

struct snake
{
    struct cs_vector points;
    struct cs_vector beziers;
};

C_END
