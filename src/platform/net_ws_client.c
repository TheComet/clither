#include "clither/platform/net.h"
#include "clither/util/log.h"
#include "clither/util/rb.h"
#include <emscripten/websocket.h>
#include <stdbool.h>

RB_DECLARE(packet_rb, struct net_packet, 16)
RB_DEFINE(packet_rb, struct net_packet, 16)

struct net_connection
{
    struct packet_rb* send_rb;
    struct packet_rb* recv_rb;

    EMSCRIPTEN_WEBSOCKET_T socket;

    unsigned open : 1;
    unsigned error : 1;
};

/* ------------------------------------------------------------------------- */
static EM_BOOL ws_open_cb(
    int event_type, const EmscriptenWebSocketOpenEvent* event, void* user_data)
{
    struct net_connection* conn = user_data;
    (void)event_type, (void)event;
    log_dbg("WebSocket connection opened\n");
    conn->open = 1;
    return EM_TRUE;
}

/* ------------------------------------------------------------------------- */
static EM_BOOL ws_close_cb(
    int event_type, const EmscriptenWebSocketCloseEvent* event, void* user_data)
{
    struct net_connection* conn = user_data;
    (void)event_type, (void)event;
    conn->open = 0;
    return EM_TRUE;
}

/* ------------------------------------------------------------------------- */
static EM_BOOL ws_error_cb(
    int event_type, const EmscriptenWebSocketErrorEvent* event, void* user_data)
{
    struct net_connection* conn = user_data;
    (void)event_type, (void)event;
    conn->error = 1;
    return EM_TRUE;
}

/* ------------------------------------------------------------------------- */
static EM_BOOL ws_msg_cb(
    int                                    event_type,
    const EmscriptenWebSocketMessageEvent* event,
    void*                                  user_data)
{
    struct net_packet*     packet;
    struct net_connection* conn = user_data;
    (void)event_type, (void)event;

    if (event->numBytes > sizeof(packet->data))
    {
        log_err("WebSocket message too large: %d\n", event->numBytes);
        return EM_FALSE;
    }

    packet = packet_rb_emplace_realloc(&conn->recv_rb);
    if (packet == NULL)
        return EM_FALSE;
    memcpy(packet->data, event->data, event->numBytes);
    packet->len = event->numBytes;

    return EM_TRUE;
}

/* ------------------------------------------------------------------------- */
static struct net_connection*
ws_client_create(const char* address, const char* port)
{
    struct net_connection*              conn;
    EmscriptenWebSocketCreateAttributes create_attr;
    (void)port;

    if (emscripten_websocket_is_supported() != EM_TRUE)
    {
        log_err("WebSockets are not supported!\n");
        return NULL;
    }

    conn = mem_alloc(sizeof(*conn));
    if (!conn)
        goto alloc_conn_failed;

    emscripten_websocket_init_create_attributes(&create_attr);
    create_attr.url = address;
    create_attr.protocols = NULL;
    create_attr.createOnMainThread = EM_FALSE;
    log_dbg("Attempting to connect websocket %s...\n", address);
    conn->socket = emscripten_websocket_new(&create_attr);
    if (conn->socket <= 0)
    {
        log_err("emscripten_websocket_new(): Failed to create socket\n");
        goto connect_failed;
    }

    packet_rb_init(&conn->send_rb);
    packet_rb_init(&conn->recv_rb);

    conn->open = 0;
    conn->error = 0;

    emscripten_websocket_set_onopen_callback(conn->socket, conn, ws_open_cb);
    emscripten_websocket_set_onclose_callback(conn->socket, conn, ws_close_cb);
    emscripten_websocket_set_onerror_callback(conn->socket, conn, ws_error_cb);
    emscripten_websocket_set_onmessage_callback(conn->socket, conn, ws_msg_cb);

    return conn;

connect_failed:
    mem_free(conn);
alloc_conn_failed:
    return NULL;
}

static void ws_client_destroy(struct net_connection* conn)
{
    emscripten_websocket_delete(conn->socket);
    packet_rb_deinit(conn->recv_rb);
    packet_rb_deinit(conn->send_rb);
    mem_free(conn);
}

static enum net_receive_result
ws_client_receive(struct net_connection* conn, struct net_packet* packet)
{
    if (conn->error)
        return -1;

    if (rb_count(conn->recv_rb) == 0)
        return 0;

    *packet = packet_rb_take(conn->recv_rb);
    return packet->len;
}

static int
ws_client_send(struct net_connection* conn, const struct net_packet* packet)
{
    int i;

    if (conn->error)
        return -1;

    if (conn->open == 0)
    {
        if (packet_rb_put_realloc(&conn->send_rb, *packet) != 0)
            return -1;
        return 0;
    }

    rb_for_each (conn->send_rb, i, packet)
        emscripten_websocket_send_binary(
            conn->socket, (void*)packet->data, packet->len);
    packet_rb_clear(conn->send_rb);

    emscripten_websocket_send_binary(
        conn->socket, (void*)packet->data, packet->len);
    return 0;
}

const struct net_client_interface net_ws_client = {
    ws_client_create, ws_client_destroy, ws_client_receive, ws_client_send};
