#pragma once

#include <stdint.h>

struct bezier_handle
{
    uint16_t x, y;
    uint8_t angle, length;
};
