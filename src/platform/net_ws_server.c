#include "clither/platform/net.h"
#include "clither/util/hmap.h"

#define MESSAGE_LENGTH 2048
struct ws_frame_data
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
    /*! \brief Client connection structure. */
    struct ws_connection* client;
};

struct ws_connection
{
    int client_sock; /**< Client socket FD.        */
    int state;       /**< WebSocket current state. */

    /* IP address and port. */
    struct net_addr addr;

    /* Ping/Pong IDs and locks. */
    int32_t last_pong_id;
    int32_t current_ping_id;

    /* Connection context */
    void* connection_context;
};

struct connection_hmap_kvs
{
    struct net_addr*      keys;
    struct ws_connection* values;
};

HMAP_DECLARE_FULL(
    static,
    connection_hmap,
    hash32,
    const struct net_addr*,
    struct ws_connection,
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

static struct ws_connection* connection_hmap_kvs_get_value(
    const struct connection_hmap_kvs* kvs, int16_t slot)
{
    return &kvs->values[slot];
}

static void connection_hmap_kvs_set_value(
    struct connection_hmap_kvs* kvs,
    int16_t                     slot,
    const struct ws_connection* value)
{
    kvs->values[slot] = *value;
}

HMAP_DEFINE_FULL(
    static,
    connection_hmap,
    hash32,
    const struct net_addr*,
    struct ws_connection,
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
};

static int ws_init(void)
{
    return 0;
}

static void ws_deinit(void)
{
}

static struct net_server*
ws_server_create(const char* address, const char* port)
{
    struct net_server* server = mem_alloc(sizeof(*server));
    if (!server)
        goto alloc_server_failed;

    server->socket = net_host_tcp(address, port);
    if (server->socket < 0)
        goto bind_socket_failed;

    connection_hmap_init(&server->connections);
    return server;

bind_socket_failed:
    mem_free(server);
alloc_server_failed:
    return NULL;
}

static void ws_server_destroy(struct net_server* server)
{
    connection_hmap_deinit(server->connections);
    net_close(server->socket);
    mem_free(server);
}

static void
ws_server_disconnect(struct net_server* server, const struct net_addr* addr)
{
    struct ws_connection* client =
        connection_hmap_find(server->connections, addr);
    CLITHER_DEBUG_ASSERT(client != NULL);
    net_close(client->client_sock);
}

static int ws_server_receive(
    struct net_server* server, struct net_addr* addr, struct net_packet* packet)
{
    return -1;
}

static int ws_server_send(
    struct net_server*       server,
    const struct net_addr*   addr,
    const struct net_packet* packet)
{
    return -1;
}

const struct net_server_interface net_ws_server = {
    ws_server_create,
    ws_server_destroy,
    ws_server_disconnect,
    ws_server_receive,
    ws_server_send};
