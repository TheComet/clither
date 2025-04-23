#include "clither/platform/net.h"
#include "clither/util/base64.h"
#include "clither/util/hmap.h"
#include "clither/util/sha1.h"
#include <ctype.h>
#include <stdio.h>

enum connection_state
{
    STATE_CONNECTING = 0,
    STATE_OPEN,
    STATE_CLOSING,
    STATE_CLOSED
};

#define MESSAGE_LENGTH 2048
struct frame_data
{
    /*! \brief Frame read. */
    unsigned char frm[MESSAGE_LENGTH];
    /*! \brief Processed message at the moment. */
    unsigned char* msg;
    /*! \brief Control frame payload */
    unsigned char msg_ctrl[125];
    /*! \brief Current byte position. */
    size_t cur_pos;
    /*! \brief Amount of read bytes. */
    size_t amt_read;
    /*! \brief Frame type, like text or binary. */
    int frame_type;
    /*! \brief Frame size. */
    uint64_t frame_size;
    /*! \brief Error flag, set when a read was not possible. */
    int error;
};

/*!
 * Because our receive() function is non-blocking, we need to be able to return
 * and resume parsing as the header crosses packet boundaries.
 *
 * "char_state" keeps track of the next character that is expected. For
 * example, in order to parse the string "Sec-WebSocket-Key", the state machine
 * will go through 17 state transitions.
 *
 * "token_state" keeps track of the next expected token. Tokens are combinations
 * of characters.
 */
struct http_upgrade_parser
{
    int      char_state;
    int      token_state;
    unsigned found_websocket_upgrade : 1;
    unsigned found_connection_upgrade : 1;
    unsigned found_websocket_key : 1;
};

struct connection
{
    struct frame_data          frame_data;
    struct http_upgrade_parser http_header_parser;

    int socket;

    int32_t last_pong_id;
    int32_t current_ping_id;

    enum connection_state state;
};

static void frame_data_init(struct frame_data* frame_data)
{
    frame_data->msg = NULL;
    frame_data->cur_pos = 0;
    frame_data->amt_read = 0;
    frame_data->frame_type = 0;
    frame_data->frame_size = 0;
    frame_data->error = 0;
}

static void http_header_parser_init(struct http_upgrade_parser* parser)
{
    parser->char_state = 0;
    parser->token_state = 0;
}

static void connection_init(struct connection* connection, int socket)
{
    connection->socket = socket;
    connection->state = STATE_CONNECTING;
    connection->last_pong_id = 0;
    connection->current_ping_id = 0;

    frame_data_init(&connection->frame_data);
    http_header_parser_init(&connection->http_header_parser);
}

struct connection_hmap_kvs
{
    struct net_addr*   keys;
    struct connection* values;
};

HMAP_DECLARE_FULL(
    static,
    connection_hmap,
    hash32,
    const struct net_addr*,
    struct connection,
    16,
    struct connection_hmap_kvs)

static int connection_hmap_kvs_alloc(
    struct connection_hmap_kvs* kvs,
    struct connection_hmap_kvs* old_kvs,
    int16_t                     capacity)
{
    (void)old_kvs;
    kvs->keys = mem_alloc(sizeof(*kvs->keys) * capacity);
    if (kvs->keys == NULL)
        goto alloc_keys_failed;

    kvs->values = mem_alloc(sizeof(*kvs->values) * capacity);
    if (kvs->values == NULL)
        goto alloc_values_failed;

    return 0;
alloc_values_failed:
    mem_free(kvs->keys);
alloc_keys_failed:
    return -1;
}

static void connection_hmap_kvs_free(struct connection_hmap_kvs* kvs)
{
    mem_free(kvs->values);
    mem_free(kvs->keys);
}

static void connection_hmap_kvs_free_old(struct connection_hmap_kvs* kvs)
{
    connection_hmap_kvs_free(kvs);
}

static hash32 connection_hmap_kvs_hash(const struct net_addr* key)
{
    return hash32_jenkins_oaat(key->sockaddr_storage, key->len);
}

static const struct net_addr*
connection_hmap_kvs_get_key(const struct connection_hmap_kvs* kvs, int16_t slot)
{
    return &kvs->keys[slot];
}

static void connection_hmap_kvs_set_key(
    struct connection_hmap_kvs* kvs, int16_t slot, const struct net_addr* key)
{
    kvs->keys[slot].len = key->len;
    memcpy(kvs->keys[slot].sockaddr_storage, key->sockaddr_storage, key->len);
}

static int connection_hmap_kvs_keys_equal(
    const struct net_addr* k1, const struct net_addr* k2)
{
    return k1->len == k2->len && memcmp(k1, k2, k1->len) == 0;
}

static struct connection* connection_hmap_kvs_get_value(
    const struct connection_hmap_kvs* kvs, int16_t slot)
{
    return &kvs->values[slot];
}

static void connection_hmap_kvs_set_value(
    struct connection_hmap_kvs* kvs,
    int16_t                     slot,
    const struct connection*    value)
{
    kvs->values[slot] = *value;
}

HMAP_DEFINE_FULL(
    static,
    connection_hmap,
    hash32,
    const struct net_addr*,
    struct connection,
    16,
    connection_hmap_kvs_hash,
    connection_hmap_kvs_alloc,
    connection_hmap_kvs_free_old,
    connection_hmap_kvs_free,
    connection_hmap_kvs_get_key,
    connection_hmap_kvs_set_key,
    connection_hmap_kvs_keys_equal,
    connection_hmap_kvs_get_value,
    connection_hmap_kvs_set_value,
    16,
    70)

struct net_server
{
    struct connection_hmap* connections;
    int                     socket;
    int16_t                 iter_conn_slot;
};

static struct net_server*
ws_server_create(const char* address, const char* port)
{
    struct net_server* server = mem_alloc(sizeof(*server));
    if (!server)
        goto alloc_server_failed;

    server->socket = net_host_tcp(address, port);
    if (server->socket < 0)
        goto bind_socket_failed;

    server->iter_conn_slot = -1;
    connection_hmap_init(&server->connections);
    return server;

bind_socket_failed:
    mem_free(server);
alloc_server_failed:
    return NULL;
}

static void ws_server_destroy(struct net_server* server)
{
    int16_t            slot;
    struct net_addr    addr;
    struct connection* client;

    hmap_for_each (server->connections, slot, addr, client)
        (void)slot, (void)addr, net_close(client->socket);
    connection_hmap_deinit(server->connections);

    net_close(server->socket);
    mem_free(server);
}

static void
ws_server_disconnect(struct net_server* server, const struct net_addr* addr)
{
    struct connection* client =
        connection_hmap_erase(server->connections, addr);
    CLITHER_DEBUG_ASSERT(client != NULL);
    net_close(client->socket);
}

static int
accept_new_connections(struct net_server* server, struct net_addr* addr)
{
    struct connection* conn;
    int                fd;

    fd = net_accept(server->socket, addr);
    if (fd <= 0)
        return fd;
    if (net_set_nonblock_reuse(fd) < 0)
    {
        net_close(fd);
        return -1;
    }

    switch (connection_hmap_emplace_or_get(&server->connections, addr, &conn))
    {
        case HMAP_OOM: return -1;
        case HMAP_EXISTS: break;
        case HMAP_NEW: {
            connection_init(conn, fd);
            break;
        }
    }

    return 0;
}

enum handshake_result
{
    HANDSHAKE_NEED_MORE_DATA = -1,
    HANDSHAKE_SUCCESS = 0,
    HANDSHAKE_ERROR
};

#define B64_KEY_SIZE 24 /* incoming base64 key is always 24 chars long */
static enum handshake_result parse_http_websocket_upgrade_request(
    struct http_upgrade_parser* p,
    const struct net_packet*    packet,
    char                        key[B64_KEY_SIZE + 1])
{
    static const char handshake_key[] = "sec-websocket-key: ";
    static const char upgrade_key[] = "upgrade: ";
    static const char connection_key[] = "connection: ";

    enum token_state
    {
        EXPECT_GET,
        EXPECT_VERSION_AND_CRLF,
        EXPECT_KEY_OR_END,
        EXPECT_KEY_HANDSHAKE,
        EXPECT_VALUE_HANDSHAKE,
        EXPECT_KEY_UPGRADE,
        EXPECT_VALUE_UPGRADE,
        EXPECT_KEY_CONNECTION,
        EXPECT_VALUE_CONNECTION,
        IGNORE_UNTIL_CRLF,
        EXPECT_END
    };

    int off = 0;
reswitch:
    switch ((enum token_state)p->token_state)
    {
        /* Parse GET request beginning */
        case EXPECT_GET: {
            static const char request[] = "get / http/";
            while (off != packet->len && p->char_state != sizeof(request) - 1)
                if (tolower(packet->data[off++]) != request[p->char_state++])
                    return HANDSHAKE_ERROR;
            if (off == packet->len)
                return HANDSHAKE_NEED_MORE_DATA;

            p->char_state = 0;
            p->token_state = EXPECT_VERSION_AND_CRLF;
        } /* fallthrough */

        /* HTTP version doesn't matter and ensure GET request ends with \r\n */
        case EXPECT_VERSION_AND_CRLF: {
            while (off != packet->len && p->char_state != 2)
            {
                if (p->char_state == 0 &&
                    (isdigit(packet->data[off]) || packet->data[off] == '.'))
                {
                    off++;
                    continue;
                }
                if (packet->data[off++] != "\r\n"[p->char_state++])
                    return HANDSHAKE_ERROR;
            }
            if (off == packet->len)
                return HANDSHAKE_NEED_MORE_DATA;

            p->char_state = 0;
            p->token_state = EXPECT_KEY_OR_END;
        } /* fallthrough */

        case EXPECT_KEY_OR_END: {
            if (tolower(packet->data[off]) == handshake_key[0])
                p->token_state = EXPECT_KEY_HANDSHAKE;
            else if (tolower(packet->data[off]) == upgrade_key[0])
                p->token_state = EXPECT_KEY_UPGRADE;
            else if (tolower(packet->data[off]) == connection_key[0])
                p->token_state = EXPECT_KEY_CONNECTION;
            else if (packet->data[off] == '\r')
                p->token_state = EXPECT_END;
            else
                p->token_state = IGNORE_UNTIL_CRLF;

            goto reswitch;
        }

        case EXPECT_KEY_HANDSHAKE: {
            while (off != packet->len &&
                   p->char_state != sizeof(handshake_key) - 1)
            {
                if (tolower(packet->data[off++]) !=
                    handshake_key[p->char_state++])
                    break;
            }
            if (off == packet->len)
                return HANDSHAKE_NEED_MORE_DATA;

            p->token_state = p->char_state == sizeof(handshake_key) - 1
                                 ? EXPECT_VALUE_HANDSHAKE
                                 : IGNORE_UNTIL_CRLF;
            p->char_state = 0;
            goto reswitch;
        }

        case EXPECT_VALUE_HANDSHAKE: {
            while (off != packet->len && p->char_state != B64_KEY_SIZE)
                key[p->char_state++] = packet->data[off++];
            if (off == packet->len)
                return HANDSHAKE_NEED_MORE_DATA;
            if (packet->data[off] != '\r')
            {
                log_err(
                    "The received Sec-WebSocket-Key has a different size than "
                    "expected.\n");
                log_hex_ascii(packet->data, packet->len);
                return HANDSHAKE_ERROR;
            }

            key[p->char_state] = '\0';
            p->found_websocket_key = 1;
            p->char_state = 0;
            p->token_state = IGNORE_UNTIL_CRLF;
            goto reswitch;
        }

        case EXPECT_KEY_UPGRADE: {
            while (off != packet->len &&
                   p->char_state != sizeof(upgrade_key) - 1)
            {
                if (tolower(packet->data[off++]) !=
                    upgrade_key[p->char_state++])
                    break;
            }
            if (off == packet->len)
                return HANDSHAKE_NEED_MORE_DATA;

            p->token_state = p->char_state == sizeof(upgrade_key) - 1
                                 ? EXPECT_VALUE_UPGRADE
                                 : IGNORE_UNTIL_CRLF;
            p->char_state = 0;
            goto reswitch;
        }

        case EXPECT_VALUE_UPGRADE: {
            p->found_websocket_upgrade = 1;
            p->token_state = IGNORE_UNTIL_CRLF;
            goto reswitch;
        }

        case EXPECT_KEY_CONNECTION: {
            while (off != packet->len &&
                   p->char_state != sizeof(connection_key) - 1)
            {
                if (tolower(packet->data[off++]) !=
                    connection_key[p->char_state++])
                    break;
            }
            if (off == packet->len)
                return HANDSHAKE_NEED_MORE_DATA;

            p->token_state = p->char_state == sizeof(connection_key) - 1
                                 ? EXPECT_VALUE_CONNECTION
                                 : IGNORE_UNTIL_CRLF;
            p->char_state = 0;
            goto reswitch;
        }

        case EXPECT_VALUE_CONNECTION: {
            p->found_connection_upgrade = 1;
            p->token_state = IGNORE_UNTIL_CRLF;
            goto reswitch;
        }

        case IGNORE_UNTIL_CRLF: {
            while (off != packet->len && p->char_state != 2)
            {
                if (p->char_state == 0 && packet->data[off] != '\r')
                {
                    off++;
                    continue;
                }
                if (packet->data[off++] != "\r\n"[p->char_state++])
                    return HANDSHAKE_ERROR;
            }
            if (off == packet->len)
                return HANDSHAKE_NEED_MORE_DATA;
            p->char_state = 0;
            p->token_state = EXPECT_KEY_OR_END;
            goto reswitch;
        }

        case EXPECT_END: {
            while (off != packet->len && p->char_state != 2)
                if (packet->data[off++] != "\r\n"[p->char_state++])
                    return HANDSHAKE_ERROR;
            break;
        }
    }

    return (p->token_state == EXPECT_END && p->found_websocket_upgrade &&
            p->found_connection_upgrade && p->found_websocket_key)
               ? HANDSHAKE_SUCCESS
               : HANDSHAKE_ERROR;
}

static enum handshake_result send_handshake_response(
    struct connection* conn, const struct net_packet* packet)
{
#define UUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define ACCEPT                                                                 \
    "HTTP/1.1 101 Switching Protocols\r\n"                                     \
    "Upgrade: websocket\r\n"                                                   \
    "Connection: Upgrade\r\n"                                                  \
    "Sec-WebSocket-Accept: "
#define BAD_REQUEST                                                            \
    "HTTP/1.1 400 Bad Request\r\n"                                             \
    "Connection: close\r\n\r\n"

    char key[B64_KEY_SIZE + sizeof(UUID)];
    switch (parse_http_websocket_upgrade_request(
        &conn->http_header_parser, packet, key))
    {
        case HANDSHAKE_NEED_MORE_DATA: return HANDSHAKE_NEED_MORE_DATA;

        case HANDSHAKE_ERROR: {
            net_send(conn->socket, BAD_REQUEST, sizeof(BAD_REQUEST) - 1);
            return HANDSHAKE_ERROR;
        }

        case HANDSHAKE_SUCCESS: {
            SHA1Context ctx;
            char        accept_msg
                [sizeof(ACCEPT) + base64_output_len(SHA1HashSize) + 4];
            /* This includes the null terminator */
            strcat(key, UUID);

            SHA1Reset(&ctx);
            SHA1Input(&ctx, (const uint8_t*)key, strlen(key));
            CLITHER_STATIC_ASSERT(sizeof(key) >= SHA1HashSize);
            SHA1Result(&ctx, (uint8_t*)key);

            memcpy(accept_msg, ACCEPT, sizeof(ACCEPT) - 1);
            base64_encode(/* Adds a null terminator */
                          (uint8_t*)(accept_msg + sizeof(ACCEPT) - 1),
                          (const uint8_t*)key,
                          SHA1HashSize);
            strcat(accept_msg, "\r\n\r\n");
            CLITHER_DEBUG_ASSERT(strlen(accept_msg) < sizeof(accept_msg));

            net_send(conn->socket, accept_msg, strlen(accept_msg));

            return HANDSHAKE_SUCCESS;
#undef B64_KEY_SIZE
#undef ACCEPT
        }
    }

    return HANDSHAKE_ERROR;
}

static enum net_receive_result ws_server_receive(
    struct net_server* server, struct net_addr* addr, struct net_packet* packet)
{
    struct connection* conn;

    if (server->iter_conn_slot == -1)
        if (accept_new_connections(server, addr) != 0)
            return NET_RECEIVE_ERROR;

recv_next_connection:
    server->iter_conn_slot = hmap_next_valid_slot(
        server->connections->hashes,
        server->iter_conn_slot,
        hmap_capacity(server->connections));
    if (server->iter_conn_slot == hmap_capacity(server->connections))
    {
        server->iter_conn_slot = -1;
        return NET_RECEIVE_NO_DATA;
    }

    conn = connection_hmap_kvs_get_value(
        &server->connections->kvs, server->iter_conn_slot);

recv_next_chunk:
    packet->len = net_recv(conn->socket, packet->data, sizeof(packet->data));

    if (packet->len < 0)
        return NET_RECEIVE_ERROR;

    if (packet->len == 0)
        goto recv_next_connection;

    switch (conn->state)
    {
        case STATE_CONNECTING: {
            switch (send_handshake_response(conn, packet))
            {
                case HANDSHAKE_NEED_MORE_DATA: goto recv_next_chunk;
                case HANDSHAKE_SUCCESS:
                    conn->state = STATE_OPEN;
                    log_dbg("WebSocket handshake complete\n");
                    goto recv_next_chunk;
                case HANDSHAKE_ERROR:
                    log_err("WebSocket handshake failed\n");
                    net_close(conn->socket);
                    connection_hmap_erase_slot(
                        server->connections, server->iter_conn_slot);
                    goto recv_next_connection;
            }
            break;
        }

        case STATE_OPEN: {
            log_dbg("WebSocket message received\n");
            log_hex_ascii(packet->data, packet->len);
            break;
        }

        case STATE_CLOSING:
        case STATE_CLOSED: break;
    }

    goto recv_next_chunk;
}

static int ws_server_send(
    struct net_server*       server,
    const struct net_addr*   addr,
    const struct net_packet* packet)
{
    struct connection* conn = connection_hmap_find(server->connections, addr);
    CLITHER_DEBUG_ASSERT(conn != NULL);

    return net_send(conn->socket, packet->data, packet->len);
}

const struct net_server_interface net_ws_server = {
    ws_server_create,
    ws_server_destroy,
    ws_server_disconnect,
    ws_server_receive,
    ws_server_send};
