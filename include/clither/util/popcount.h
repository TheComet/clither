#pragma once

#include "clither/config.h"
#include <stdint.h>

#if !defined(CLITHER_POPCOUNT)
static uint32_t CLITHER_POPCOUNT(uint32_t value)
{
    uint32_t count = 0;
    while (value)
    {
        if (value & 1)
            count++;
        value >>= 1;
    }
    return count;
}
#endif
