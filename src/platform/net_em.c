#include "clither/platform/net.h"
#include <emscripten/websocket.h>

/* ------------------------------------------------------------------------- */
int net_init(void)
{
    if (emscripten_websocket_is_supported() != EM_TRUE)
    {
        log_err("WebSockets are not supported!\n");
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
void net_deinit(void)
{
    emscripten_websocket_deinitialize();
}
