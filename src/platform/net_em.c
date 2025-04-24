#include "clither/platform/net.h"
#include "clither/util/log.h"
#include "clither/util/rb.h"
#include "clither/util/vec.h"
#include <assert.h>
#include <emscripten/websocket.h>

VEC_DEFINE(sockfd_vec, int, 8)

RB_DECLARE(packet_rb, struct net_packet, 16)
RB_DEFINE(packet_rb, struct net_packet, 16)

struct state
{
    struct packet_rb* send_rb;
    struct packet_rb* recv_rb;

    unsigned connecting : 1;
    unsigned open : 1;
    unsigned error : 1;
};

static struct state g_state;

/* ------------------------------------------------------------------------- */
static EM_BOOL ws_open_cb(
    int event_type, const EmscriptenWebSocketOpenEvent* event, void* user_data)
{
    (void)event_type, (void)event, (void)user_data;
    g_state.open = 1;
    return EM_TRUE;
}

/* ------------------------------------------------------------------------- */
static EM_BOOL ws_close_cb(
    int event_type, const EmscriptenWebSocketCloseEvent* event, void* user_data)
{
    (void)event_type, (void)event, (void)user_data;
    g_state.open = 0;
    g_state.connecting = 0;
    return EM_TRUE;
}

/* ------------------------------------------------------------------------- */
static EM_BOOL ws_error_cb(
    int event_type, const EmscriptenWebSocketErrorEvent* event, void* user_data)
{
    (void)event_type, (void)event, (void)user_data;
    g_state.error = 1;
    return EM_TRUE;
}

/* ------------------------------------------------------------------------- */
static EM_BOOL ws_msg_cb(
    int                                    event_type,
    const EmscriptenWebSocketMessageEvent* event,
    void*                                  user_data)
{
    struct net_packet* packet;
    (void)event_type, (void)event, (void)user_data;

    if (event->numBytes > sizeof(struct net_packet))
    {
        log_err("WebSocket message too large: %d\n", event->numBytes);
        return EM_FALSE;
    }

    packet = packet_rb_emplace_realloc(&g_state.recv_rb);
    if (packet == NULL)
        return EM_FALSE;
    memcpy(packet->data, event->data, event->numBytes);
    packet->len = event->numBytes;

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

    packet_rb_init(&g_state.send_rb);
    packet_rb_init(&g_state.recv_rb);

    g_state.connecting = 0;
    g_state.open = 0;
    g_state.error = 0;

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
    (void)addr;
    str->cstr[0] = '\0';
}

/* ------------------------------------------------------------------------- */
int net_connect_udp(
    struct sockfd_vec** sockfds, const char* server_address, const char* port)
{
    int                                 sockfd;
    EmscriptenWebSocketCreateAttributes create_attr;
    (void)port;

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

    g_state.error = 0;
    g_state.connecting = 1;
    g_state.open = 0;

    emscripten_websocket_set_onopen_callback(sockfd, NULL, ws_open_cb);
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
    (void)sockfd, (void)addr, (void)buf, (void)len;
    CLITHER_DEBUG_ASSERT(0);
    return -1;
}

/* ------------------------------------------------------------------------- */
int net_send(int sockfd, const void* buf, int len)
{
    if (g_state.error || g_state.connecting == 0)
        return -1;

    if (g_state.open)
    {
        int                i;
        struct net_packet* packet;
        rb_for_each (g_state.send_rb, i, packet)
            emscripten_websocket_send_binary(sockfd, packet->data, packet->len);
        packet_rb_clear(g_state.send_rb);

        emscripten_websocket_send_binary(sockfd, (void*)buf, len);
    }
    else
    {
        struct net_packet* packet = packet_rb_emplace_realloc(&g_state.send_rb);
        if (packet == NULL)
            return -1;
        memcpy(packet->data, buf, len);
        packet->len = len;
    }

    return len;
}

/* ------------------------------------------------------------------------- */
int net_recvfrom(int sockfd, struct net_addr* addr, void* buf, int capacity)
{
    (void)sockfd, (void)addr, (void)buf, (void)capacity;
    CLITHER_DEBUG_ASSERT(0);
    return -1;
}

/* ------------------------------------------------------------------------- */
int net_recv(int sockfd, void* buf, int capacity)
{
    struct net_packet* packet;
    int                len;
    (void)sockfd;

    if (g_state.error || g_state.connecting == 0)
        return -1;

    if (rb_count(g_state.recv_rb) == 0)
        return 0;

    if (capacity < (int)sizeof(packet->data))
        return log_err("Buffer too small: %d\n", capacity);

    packet = rb_peek_read(g_state.recv_rb);
    memcpy(buf, packet->data, packet->len);
    len = packet->len;
    packet_rb_take(g_state.recv_rb);

    return len;
}
