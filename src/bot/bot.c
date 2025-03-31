#include "clither/bot/bot.h"
#include <stddef.h>

#if defined(CLITHER_BOT_API_LUA)
extern const struct bot_interface bot_lua;
#endif

const struct bot_interface* bot_backends[] = {
#if defined(CLITHER_BOT_API_LUA)
    &bot_lua,
#endif
    NULL
};
