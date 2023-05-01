#pragma once

#include "clither/config.h"
#include "cstructures/vector.h"

C_BEGIN

struct pos2
{

};

struct snake
{
    struct cs_vector points;
    struct cs_vector beziers;
};

C_END
