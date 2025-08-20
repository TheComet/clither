#pragma once

#include "c-ini.h"
#include <stdint.h>

SECTION("client")
struct settings_client
{
    char* username DEFAULT("Snek :D");
    char           address[128] DEFAULT("127.0.0.1");
    uint16_t port  DEFAULT(8080) CONSTRAIN(1, 65535);
};
