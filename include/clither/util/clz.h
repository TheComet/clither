#pragma once

#include "clither/config.h"
#include <assert.h>
#include <stdint.h>

#if !defined(CLITHER_CLZ)
static uint32_t CLITHER_CLZ(uint32_t value)
{
    uint32_t count = 0;
    CLITHER_DEBUG_ASSERT(value != 0);
    while (value)
    {
        if (value & 0x80000000)
            break;
        count++;
        value <<= 1;
    }
    return count;
}
#endif
