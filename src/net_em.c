#include "clither/log.h"
#include "clither/net.h"

#include "cstructures/vector.h"

#include <emscripten/websocket.h>

/* ------------------------------------------------------------------------- */
static EM_BOOL
ws_open_cb(int event_type, const EmscriptenWebSocketOpenEvent* event, void* user_data)
{
    return EM_TRUE;
}

/* ------------------------------------------------------------------------- */
static EM_BOOL
ws_close_cb(int event_type, const EmscriptenWebSocketCloseEvent* event, void* user_data)
{
    return EM_TRUE;
}

/* ------------------------------------------------------------------------- */
static EM_BOOL
ws_error_cb(int event_type, const EmscriptenWebSocketErrorEvent* event, void* user_data)
{
    return EM_TRUE;
}

/* ------------------------------------------------------------------------- */
static EM_BOOL
ws_msg_cb(int event_type, const EmscriptenWebSocketMessageEvent* event, void* user_data)
{
    return EM_TRUE;
}

/* ------------------------------------------------------------------------- */
int
net_init(void)
{
    if (emscripten_websocket_is_supported() != EM_TRUE)
    {
        log_err("WebSockets are not supported!\n");
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
void
net_deinit(void)
{
    emscripten_websocket_deinitialize();
}

/* ------------------------------------------------------------------------- */
void
net_log_host_ips(void)
{}

/* ------------------------------------------------------------------------- */
void
net_addr_to_str(char* str, int len, const void* addr)
{
    str[0] = '\0';
}

/* ------------------------------------------------------------------------- */
int
net_bind(const char* bind_address, const char* port, int* addrlen)
{
    assert(0);
    return -1;
}

/* ------------------------------------------------------------------------- */
int
net_connect(struct cs_vector* sockfds, const char* server_address, const char* port)
{
    int sockfd;

    EmscriptenWebSocketCreateAttributes create_attr;
    emscripten_websocket_init_create_attributes(&create_attr);
    create_attr.url = server_address;
    /* XXX: For some reason setting this to "binary" makes it not work */
    /* XXX: Where is the documentation for this shit? */
    create_attr.protocols = NULL; /*"binary";*/
    sockfd = emscripten_websocket_new(&create_attr);
    if (sockfd <= 0)
    {
        log_err("emscripten_websocket_new(): Failed to create socket\n");
        return -1;
    }

    emscripten_websocket_set_onopen_callback(sockfd, NULL, ws_open_cb);
    emscripten_websocket_set_onclose_callback(sockfd, NULL, ws_close_cb);
    emscripten_websocket_set_onerror_callback(sockfd, NULL, ws_error_cb);
    emscripten_websocket_set_onmessage_callback(sockfd, NULL, ws_msg_cb);

    vector_push(sockfds, &sockfd);

    return 0;
}

/* ------------------------------------------------------------------------- */
void
net_close(int sockfd)
{
    emscripten_websocket_delete(sockfd);
}

/* ------------------------------------------------------------------------- */
int
net_sendto(int sockfd, const void* buf, int len, const void* addr, int addrlen)
{
    assert(0);
    return -1;
}

/* ------------------------------------------------------------------------- */
int
net_send(int sockfd, const void* buf, int len)
{
    emscripten_websocket_send_binary(sockfd, "succ", sizeof("succ"));
    //emscripten_websocket_send_binary(sockfd, (void*)buf, len);
    return 0;
}

/* ------------------------------------------------------------------------- */
int
net_recvfrom(int sockfd, void* buf, int capacity, void* addr, int addrlen)
{
    assert(0);
    return -1;
}

/* ------------------------------------------------------------------------- */
int
net_recv(int sockfd, void* buf, int capacity)
{
    return 0;
}
