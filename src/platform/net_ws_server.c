#include "clither/platform/net.h"
#include "clither/util/base64.h"
#include "clither/util/hmap.h"
#include "clither/util/sha1.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>

enum connection_state
{
    STATE_CONNECTING = 0,
    STATE_OPEN,
    STATE_CLOSING,
    STATE_CLOSED
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
#define B64_KEY_SIZE 24 /* incoming base64 key is always 24 chars long */
#define UUID         "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
struct http_upgrade_parser
{
    char websocket_key[B64_KEY_SIZE + sizeof(UUID)];

    int      char_state;
    int      token_state;
    unsigned found_websocket_upgrade : 1;
    unsigned found_connection_upgrade : 1;
    unsigned found_websocket_key : 1;
};

struct frame_parser
{
    uint64_t frame_length;
    uint64_t frame_idx;

    int state;

    unsigned fin : 1;
    unsigned rsv : 3;
    unsigned opcode : 4;

    unsigned mask : 1;
    unsigned payload_len : 7;

    uint8_t masks[4];
};

struct connection
{
    int socket;

    int32_t last_pong_id;
    int32_t current_ping_id;

    struct frame_parser        frame_parser;
    struct http_upgrade_parser http_header_parser;
    struct net_packet          game_packet;
    struct net_packet          ws_packet;
    int                        ws_packet_off;

    enum connection_state state;
};

static void frame_parser_init(struct frame_parser* p)
{
    p->state = 0;
}

static void http_header_parser_init(struct http_upgrade_parser* p)
{
    p->char_state = 0;
    p->token_state = 0;
    p->found_websocket_upgrade = 0;
    p->found_connection_upgrade = 0;
    p->found_websocket_key = 0;
}

static void connection_init(struct connection* conn, int socket)
{
    conn->socket = socket;
    conn->state = STATE_CONNECTING;
    conn->last_pong_id = 0;
    conn->current_ping_id = 0;
    conn->game_packet.len = 0;
    conn->ws_packet_off = 0;
    conn->ws_packet.len = 0;

    frame_parser_init(&conn->frame_parser);
    http_header_parser_init(&conn->http_header_parser);
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

static void
connection_hmap_kvs_free(struct connection_hmap_kvs* kvs, int16_t capacity)
{
    (void)capacity;
    mem_free(kvs->values);
    mem_free(kvs->keys);
}

static void
connection_hmap_kvs_free_old(struct connection_hmap_kvs* kvs, int16_t capacity)
{
    connection_hmap_kvs_free(kvs, capacity);
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

    if (net_set_nonblock_reuse(fd) != 0)
        goto set_nonblock_failed;

    switch (connection_hmap_emplace_or_get(&server->connections, addr, &conn))
    {
        case HMAP_OOM: goto add_connection_failed;
        case HMAP_EXISTS: break;
        case HMAP_NEW: {
            connection_init(conn, fd);
            break;
        }
    }

    return 0;

add_connection_failed:
set_nonblock_failed:
    net_close(fd);
    return -1;
}

enum handshake_result
{
    HANDSHAKE_ERROR = -1,
    HANDSHAKE_SUCCESS = 0,
    HANDSHAKE_NEED_MORE_DATA
};

static enum handshake_result
parse_http_websocket_upgrade_request(struct connection* conn)
{
    static const char handshake_key[] = "sec-websocket-key: ";
    static const char upgrade_key[] = "upgrade: ";
    static const char connection_key[] = "connection: ";

    struct http_upgrade_parser* p = &conn->http_header_parser;
    const struct net_packet*    packet = &conn->ws_packet;
    int*                        off = &conn->ws_packet_off;

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

reswitch:
    switch ((enum token_state)p->token_state)
    {
        /* Parse GET request beginning */
        case EXPECT_GET: {
            static const char request[] = "get / http/";
            while (*off != packet->len && p->char_state != sizeof(request) - 1)
                if (tolower(packet->data[(*off)++]) != request[p->char_state++])
                {
                    log_err("Invalid request:\n");
                    log_hex_ascii(packet->data, packet->len);
                    return HANDSHAKE_ERROR;
                }
            if (*off == packet->len)
                return HANDSHAKE_NEED_MORE_DATA;

            p->char_state = 0;
            p->token_state = EXPECT_VERSION_AND_CRLF;
        } /* fallthrough */

        /* HTTP version doesn't matter and ensure GET request ends with \r\n */
        case EXPECT_VERSION_AND_CRLF: {
            while (*off != packet->len && p->char_state != 2)
            {
                if (p->char_state == 0 &&
                    (isdigit(packet->data[*off]) || packet->data[*off] == '.'))
                {
                    (*off)++;
                    continue;
                }
                if (packet->data[(*off)++] != "\r\n"[p->char_state++])
                {
                    log_err("Invalid request:\n");
                    log_hex_ascii(packet->data, packet->len);
                    return HANDSHAKE_ERROR;
                }
            }
            if (*off == packet->len)
                return HANDSHAKE_NEED_MORE_DATA;

            p->char_state = 0;
            p->token_state = EXPECT_KEY_OR_END;
        } /* fallthrough */

        case EXPECT_KEY_OR_END: {
            if (tolower(packet->data[*off]) == handshake_key[0])
                p->token_state = EXPECT_KEY_HANDSHAKE;
            else if (tolower(packet->data[*off]) == upgrade_key[0])
                p->token_state = EXPECT_KEY_UPGRADE;
            else if (tolower(packet->data[*off]) == connection_key[0])
                p->token_state = EXPECT_KEY_CONNECTION;
            else if (packet->data[*off] == '\r')
                p->token_state = EXPECT_END;
            else
                p->token_state = IGNORE_UNTIL_CRLF;

            goto reswitch;
        }

        case EXPECT_KEY_HANDSHAKE: {
            while (*off != packet->len &&
                   p->char_state != sizeof(handshake_key) - 1)
            {
                if (tolower(packet->data[(*off)++]) !=
                    handshake_key[p->char_state++])
                    break;
            }
            if (*off == packet->len)
                return HANDSHAKE_NEED_MORE_DATA;

            p->token_state = p->char_state == sizeof(handshake_key) - 1
                                 ? EXPECT_VALUE_HANDSHAKE
                                 : IGNORE_UNTIL_CRLF;
            p->char_state = 0;
            goto reswitch;
        }

        case EXPECT_VALUE_HANDSHAKE: {
            while (*off != packet->len && p->char_state != B64_KEY_SIZE)
                p->websocket_key[p->char_state++] = packet->data[(*off)++];
            if (*off == packet->len)
                return HANDSHAKE_NEED_MORE_DATA;
            if (packet->data[*off] != '\r')
            {
                log_err(
                    "The received Sec-WebSocket-Key has a different size than "
                    "expected.\n");
                log_hex_ascii(packet->data, packet->len);
                return HANDSHAKE_ERROR;
            }

            p->websocket_key[p->char_state] = '\0';
            p->found_websocket_key = 1;
            p->char_state = 0;
            p->token_state = IGNORE_UNTIL_CRLF;
            goto reswitch;
        }

        case EXPECT_KEY_UPGRADE: {
            while (*off != packet->len &&
                   p->char_state != sizeof(upgrade_key) - 1)
            {
                if (tolower(packet->data[(*off)++]) !=
                    upgrade_key[p->char_state++])
                    break;
            }
            if (*off == packet->len)
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
            while (*off != packet->len &&
                   p->char_state != sizeof(connection_key) - 1)
            {
                if (tolower(packet->data[(*off)++]) !=
                    connection_key[p->char_state++])
                    break;
            }
            if (*off == packet->len)
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
            while (*off != packet->len && p->char_state != 2)
            {
                if (p->char_state == 0 && packet->data[*off] != '\r')
                {
                    (*off)++;
                    continue;
                }
                if (packet->data[(*off)++] != "\r\n"[p->char_state++])
                {
                    log_err("Malformed HTTP header:\n");
                    log_hex_ascii(packet->data, packet->len);
                    return HANDSHAKE_ERROR;
                }
            }
            if (*off == packet->len)
                return HANDSHAKE_NEED_MORE_DATA;
            p->char_state = 0;
            p->token_state = EXPECT_KEY_OR_END;
            goto reswitch;
        }

        case EXPECT_END: {
            while (*off != packet->len && p->char_state != 2)
                if (packet->data[(*off)++] != "\r\n"[p->char_state++])
                {
                    log_err("HTTP header not properly terminated:\n");
                    log_hex_ascii(packet->data, packet->len);
                    return HANDSHAKE_ERROR;
                }
            break;
        }
    }

    if (p->token_state == EXPECT_END && p->found_websocket_upgrade &&
        p->found_connection_upgrade && p->found_websocket_key)
    {
        return HANDSHAKE_SUCCESS;
    }

    log_err(
        "Handshake failed: token_state: %d, Upgrade: %d, Connection: %d, "
        "Sec-WebSocket-Key: %d\n",
        p->token_state,
        p->found_websocket_upgrade,
        p->found_connection_upgrade,
        p->found_websocket_key);
    log_hex_ascii(packet->data, packet->len);
    return HANDSHAKE_ERROR;
}

static enum handshake_result send_handshake_response(struct connection* conn)
{
#define ACCEPT                                                                 \
    "HTTP/1.1 101 Switching Protocols\r\n"                                     \
    "Upgrade: websocket\r\n"                                                   \
    "Connection: Upgrade\r\n"                                                  \
    "Sec-WebSocket-Accept: "
#define BAD_REQUEST                                                            \
    "HTTP/1.1 400 Bad Request\r\n"                                             \
    "Connection: close\r\n\r\n"

    switch (parse_http_websocket_upgrade_request(conn))
    {
        case HANDSHAKE_NEED_MORE_DATA:
            log_dbg("Handshake:\n");
            log_hex_ascii(conn->ws_packet.data, conn->ws_packet.len);
            return HANDSHAKE_NEED_MORE_DATA;

        case HANDSHAKE_ERROR: {
            net_send(conn->socket, BAD_REQUEST, sizeof(BAD_REQUEST) - 1);
            log_note("Sending 400 Bad Request\n");
            return HANDSHAKE_ERROR;
        }

        case HANDSHAKE_SUCCESS: {
            SHA1Context ctx;
            char        accept_msg
                [sizeof(ACCEPT) + base64_output_len(SHA1HashSize) + 4];
            char* key = conn->http_header_parser.websocket_key;

            CLITHER_DEBUG_ASSERT(strlen(key) == B64_KEY_SIZE);
            strcat(key, UUID);

            SHA1Reset(&ctx);
            SHA1Input(&ctx, (const uint8_t*)key, strlen(key));
            CLITHER_STATIC_ASSERT(
                sizeof(conn->http_header_parser.websocket_key) >= SHA1HashSize);
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

enum frame_result
{
    FRAME_ERROR = -1,
    FRAME_SUCCESS = 0,
    FRAME_NEED_MORE_DATA
};

static enum frame_result parse_frame(struct connection* conn)
{
    struct frame_parser* p = &conn->frame_parser;
    struct net_packet*   packet = &conn->ws_packet;
    struct net_packet*   game_packet = &conn->game_packet;
    int*                 off = &conn->ws_packet_off;

    enum state
    {
        FIN_RSV_OPCODE,
        PAYLOAD_LEN,
        PAYLOAD_LEN_16,
        PAYLOAD_LEN_16_1,
        PAYLOAD_LEN_64,
        PAYLOAD_LEN_64_1,
        PAYLOAD_LEN_64_2,
        PAYLOAD_LEN_64_3,
        PAYLOAD_LEN_64_4,
        PAYLOAD_LEN_64_5,
        PAYLOAD_LEN_64_6,
        PAYLOAD_LEN_64_7,
        CHECK_MASK,
        MASK,
        MASK_1,
        MASK_2,
        MASK_3,
        CHECK_OPCODE,
        BINARY_PAYLOAD
    };

reswitch:
    switch ((enum state)p->state)
    {
        case FIN_RSV_OPCODE: {
            uint8_t b = packet->data[(*off)++];
            p->fin = (b >> 7) & 1;
            p->rsv = (b >> 4) & 0x7;
            p->opcode = b & 0xF;

            p->state = PAYLOAD_LEN;
            if (*off == packet->len)
                return FRAME_NEED_MORE_DATA;
        } /* fallthrough */

        case PAYLOAD_LEN: {
            uint8_t b = packet->data[(*off)++];
            p->mask = (b >> 7) & 1;
            p->payload_len = b & 0x7F;

            if (p->payload_len == 126)
                p->state = PAYLOAD_LEN_16;
            else if (p->payload_len == 127)
                p->state = PAYLOAD_LEN_64;
            else
            {
                p->frame_length = p->payload_len;
                p->state = CHECK_MASK;
            }

            if (*off == packet->len)
                return FRAME_NEED_MORE_DATA;
            goto reswitch;
        }

        case PAYLOAD_LEN_16: {
            p->frame_length = packet->data[(*off)++] << 8;
            p->state++;
            if (*off == packet->len)
                return FRAME_NEED_MORE_DATA;
        } /* fallthrough */
        case PAYLOAD_LEN_16 + 1: {
            p->frame_length |= packet->data[(*off)++];
            p->state = CHECK_MASK;
            if (*off == packet->len)
                return FRAME_NEED_MORE_DATA;
            goto reswitch;
        }

        case PAYLOAD_LEN_64: {
            uint8_t shift;
            p->frame_length = 0;
            while (*off != packet->len && p->state != PAYLOAD_LEN_64 + 8)
            {
                case PAYLOAD_LEN_64 + 1:
                case PAYLOAD_LEN_64 + 2:
                case PAYLOAD_LEN_64 + 3:
                case PAYLOAD_LEN_64 + 4:
                case PAYLOAD_LEN_64 + 5:
                case PAYLOAD_LEN_64 + 6:
                case PAYLOAD_LEN_64 + 7:
                    shift = (PAYLOAD_LEN_64 + 7 - p->state++) * 8;
                    p->frame_length |= (uint64_t)packet->data[(*off)++]
                                       << shift;
            }
            if (*off == packet->len)
                return FRAME_NEED_MORE_DATA;
        } /* fallthrough */

        case CHECK_MASK: {
            p->frame_idx = 0;
            p->state = p->mask ? MASK : CHECK_OPCODE;
            goto reswitch;
        }

        case MASK: {
            while (*off != packet->len && p->state != CHECK_OPCODE)
            {
                case MASK + 1:
                case MASK + 2:
                case MASK + 3:
                    p->masks[p->state++ - MASK] =
                        (uint32_t)packet->data[(*off)++];
            }
            if (*off == packet->len)
                return FRAME_NEED_MORE_DATA;
        } /* fallthrough */

        case CHECK_OPCODE: {
            /* Prepare game packet for receiving the decoded payload */
            if (p->opcode == 2 /* binary */)
                conn->game_packet.len = 0;

            if (p->opcode == 0 /* continuation */ ||
                p->opcode == 2 /* binary */)
            {
                p->state = BINARY_PAYLOAD;
            }
            else
                return log_err(
                    "Unsupported WebSocket frame opcode %d\n", p->opcode);
            goto reswitch;
        }

        case BINARY_PAYLOAD: {
            while (*off != packet->len && p->frame_idx != p->frame_length &&
                   game_packet->len != sizeof(game_packet->data))
            {
                uint8_t xor = p->mask ? (p->masks[p->frame_idx++ % 4]) : 0;
                game_packet->data[game_packet->len++] =
                    packet->data[(*off)++] ^ xor;
            }

            if (game_packet->len == sizeof(game_packet->data) &&
                p->frame_idx != p->frame_length)
            {
                log_err(
                    "WebSocket frame too large (%" PRIu64 ")\n",
                    p->frame_length);
                return FRAME_ERROR;
            }

            if (p->frame_idx != p->frame_length)
                return FRAME_NEED_MORE_DATA;

            p->state = FIN_RSV_OPCODE;
            return FRAME_SUCCESS;
        }
    }

    return FRAME_ERROR;
}

static enum net_receive_result ws_server_receive(
    struct net_server* server,
    struct net_addr*   addr,
    struct net_packet* payload_packet)
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
    *addr = *connection_hmap_kvs_get_key(
        &server->connections->kvs, server->iter_conn_slot);

recv_next_chunk:
    if (conn->ws_packet_off == conn->ws_packet.len)
    {
        conn->ws_packet_off = 0;
        conn->ws_packet.len = net_recv(
            conn->socket, conn->ws_packet.data, sizeof(conn->ws_packet.data));
    }

    if (conn->ws_packet.len < 0)
        return NET_RECEIVE_ERROR;

    if (conn->ws_packet.len == 0)
        goto recv_next_connection;

    switch (conn->state)
    {
        case STATE_CONNECTING: {
            switch (send_handshake_response(conn))
            {
                case HANDSHAKE_NEED_MORE_DATA: goto recv_next_chunk;

                case HANDSHAKE_SUCCESS: {
                    int   socket = conn->socket;
                    char* key = conn->http_header_parser.websocket_key;

                    /* This is to support multiple connections from the same
                     * browser (multiple tabs). We re-use the 20 byte SHA1 key
                     */
                    addr->len = SHA1HashSize;
                    CLITHER_STATIC_ASSERT(
                        SHA1HashSize <= sizeof(addr->sockaddr_storage));
                    memcpy(addr->sockaddr_storage, key, SHA1HashSize);
                    connection_hmap_erase_slot(
                        server->connections, server->iter_conn_slot);
                    conn =
                        connection_hmap_emplace_new(&server->connections, addr);
                    connection_init(conn, socket);

                    log_dbg("WebSocket handshake complete\n");

                    conn->state = STATE_OPEN;
                    goto recv_next_connection;
                }

                case HANDSHAKE_ERROR: {
                    log_err("WebSocket handshake failed\n");
                    net_close(conn->socket);
                    connection_hmap_erase_slot(
                        server->connections, server->iter_conn_slot);
                    goto recv_next_connection;
                }
            }
            break;
        }

        case STATE_OPEN: {
            switch (parse_frame(conn))
            {
                case FRAME_NEED_MORE_DATA: goto recv_next_chunk;

                case FRAME_SUCCESS:
                    *payload_packet = conn->game_packet;
                    return NET_RECEIVE_DATA;

                case FRAME_ERROR:
                    log_err("WebSocket communication error.\n");
                    net_close(conn->socket);
                    connection_hmap_erase_slot(
                        server->connections, server->iter_conn_slot);
                    goto recv_next_connection;
            }

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
    struct connection* conn;

    /* WebSocket frame header:
     * 1 byte: FIN, RSV, opcode
     * 1 byte: mask (no mask), payload len
     * 2 bytes: extended payload len
     */
    uint8_t ws_packet[4 + sizeof(struct net_packet)];
    int     ws_packet_header_len;

    conn = connection_hmap_find(server->connections, addr);
    if (conn == NULL)
    {
        log_warn("ws_server_send(): WebSocket connection was removed.\n");
        return 0;
    }

    if (conn->state != STATE_OPEN)
        return log_err("WebSocket connection is not open\n");

    ws_packet[0] = 0x80 | 0x2; /* FIN + opcode (binary) */
    if (packet->len > 125)
    {
        ws_packet[1] = 126;
        ws_packet[2] = (packet->len >> 8) & 0xFF;
        ws_packet[3] = packet->len & 0xFF;
        ws_packet_header_len = 4;
    }
    else
    {
        ws_packet[1] = packet->len;
        ws_packet_header_len = 2;
    }

    memcpy(ws_packet + ws_packet_header_len, packet->data, packet->len);

    return net_send(
        conn->socket, ws_packet, packet->len + ws_packet_header_len);
}

const struct net_server_interface net_ws_server = {
    ws_server_create,
    ws_server_destroy,
    ws_server_disconnect,
    ws_server_receive,
    ws_server_send};
