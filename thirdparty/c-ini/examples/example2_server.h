#pragma once

#include "c-ini.h"
#include <stdint.h>

SECTION("server")
struct settings_server
{
    char*    address;
    uint16_t port CONSTRAIN(1, 65535) DEFAULT(1000);
};

