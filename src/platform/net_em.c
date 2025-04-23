#include "clither/platform/net.h"
#include "clither/util/log.h"
#include "clither/util/vec.h"
#include <assert.h>
#include <emscripten/websocket.h>

VEC_DEFINE(sockfd_vec, int, 8)

/* ------------------------------------------------------------------------- */
static EM_BOOL ws_open_cb(
    int event_type, const EmscriptenWebSocketOpenEvent* event, void* user_data)
{
    int sockfd = (int)(intptr_t)user_data;
    log_dbg("ws_open_cb\n");
    emscripten_websocket_send_binary(sockfd, "aaa", sizeof("aaa") - 1);
    emscripten_websocket_send_binary(sockfd, "bbb", sizeof("aaa") - 1);
    emscripten_websocket_send_binary(sockfd, "ccc", sizeof("aaa") - 1);
    emscripten_websocket_send_binary(sockfd, "ddd", sizeof("aaa") - 1);
    emscripten_websocket_send_binary(sockfd, "eee", sizeof("aaa") - 1);
    emscripten_websocket_send_binary(sockfd, "fff", sizeof("aaa") - 1);
    emscripten_websocket_send_binary(sockfd, "ggg", sizeof("aaa") - 1);
    return EM_TRUE;
}

/* ------------------------------------------------------------------------- */
static EM_BOOL ws_close_cb(
    int event_type, const EmscriptenWebSocketCloseEvent* event, void* user_data)
{
    log_dbg("ws_close_cb\n");
    return EM_TRUE;
}

/* ------------------------------------------------------------------------- */
static EM_BOOL ws_error_cb(
    int event_type, const EmscriptenWebSocketErrorEvent* event, void* user_data)
{
    log_dbg("ws_error_cb\n");
    return EM_TRUE;
}

/* ------------------------------------------------------------------------- */
static EM_BOOL ws_msg_cb(
    int                                    event_type,
    const EmscriptenWebSocketMessageEvent* event,
    void*                                  user_data)
{
    log_dbg("ws_msg_cb\n");
    return EM_TRUE;
}

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

/* ------------------------------------------------------------------------- */
void net_log_host_ips(void)
{
}

/* ------------------------------------------------------------------------- */
void net_addr_to_str(struct net_addr_str* str, const struct net_addr* addr)
{
    str->cstr[0] = '\0';
}

/* ------------------------------------------------------------------------- */
int net_connect_udp(
    struct sockfd_vec** sockfds, const char* server_address, const char* port)
{
    int sockfd;

    EmscriptenWebSocketCreateAttributes create_attr;
    emscripten_websocket_init_create_attributes(&create_attr);
    create_attr.url = server_address;
    create_attr.protocols = NULL;
    create_attr.createOnMainThread = EM_FALSE;
    log_dbg("Attempting to connect websocket %s...\n", server_address);
    sockfd = emscripten_websocket_new(&create_attr);
    if (sockfd <= 0)
    {
        log_err("emscripten_websocket_new(): Failed to create socket\n");
        return -1;
    }

    emscripten_websocket_set_onopen_callback(sockfd, (void*)sockfd, ws_open_cb);
    emscripten_websocket_set_onclose_callback(sockfd, NULL, ws_close_cb);
    emscripten_websocket_set_onerror_callback(sockfd, NULL, ws_error_cb);
    emscripten_websocket_set_onmessage_callback(sockfd, NULL, ws_msg_cb);

    return sockfd_vec_push(sockfds, sockfd);
}

/* ------------------------------------------------------------------------- */
void net_close(int sockfd)
{
    emscripten_websocket_delete(sockfd);
}

/* ------------------------------------------------------------------------- */
int net_sendto(
    int sockfd, const struct net_addr* addr, const void* buf, int len)
{
    assert(0);
    return -1;
}

/* ------------------------------------------------------------------------- */
int net_send(int sockfd, const void* buf, int len)
{
    // emscripten_websocket_send_binary(sockfd, (void*)buf, len);
    return 0;
}

/* ------------------------------------------------------------------------- */
int net_recvfrom(int sockfd, struct net_addr* addr, void* buf, int capacity)
{
    assert(0);
    return -1;
}

/* ------------------------------------------------------------------------- */
int net_recv(int sockfd, void* buf, int capacity)
{
    return 0;
}
