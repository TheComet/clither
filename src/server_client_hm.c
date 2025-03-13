#include "clither/net.h"
#include "clither/server_client_hm.h"

static int server_client_hm_kvs_alloc(
    struct server_client_hm_kvs* kvs,
    struct server_client_hm_kvs* old_kvs,
    int16_t                      capacity)
{
    (void)old_kvs;
    kvs->keys = mem_alloc(sizeof(struct net_addr) * capacity);
    if (kvs->keys == NULL)
        return -1;

    kvs->values = mem_alloc(sizeof(struct server_client) * capacity);
    if (kvs->values == NULL)
    {
        mem_free(kvs->keys);
        return -1;
    }

    return 0;
}

static void server_client_hm_kvs_free(struct server_client_hm_kvs* kvs)
{
    mem_free(kvs->values);
    mem_free(kvs->keys);
}

static void server_client_hm_kvs_free_old(struct server_client_hm_kvs* kvs)
{
    server_client_hm_kvs_free(kvs);
}

static hash32 server_client_hm_kvs_hash(const struct net_addr* key)
{
    return hash32_jenkins_oaat(key->sockaddr_storage, key->len);
}

const struct net_addr* server_client_hm_kvs_get_key(
    const struct server_client_hm_kvs* kvs, int16_t slot)
{
    return &kvs->keys[slot];
}

static void server_client_hm_kvs_set_key(
    struct server_client_hm_kvs* kvs, int16_t slot, const struct net_addr* key)
{
    kvs->keys[slot].len = key->len;
    memcpy(kvs->keys[slot].sockaddr_storage, key->sockaddr_storage, key->len);
}

static int server_client_hm_kvs_keys_equal(
    const struct net_addr* k1, const struct net_addr* k2)
{
    return k1->len == k2->len && memcmp(k1, k2, k1->len) == 0;
}

struct server_client* server_client_hm_kvs_get_value(
    const struct server_client_hm_kvs* kvs, int16_t slot)
{
    return &kvs->values[slot];
}

static void server_client_hm_kvs_set_value(
    struct server_client_hm_kvs* kvs,
    int16_t                      slot,
    const struct server_client*  value)
{
    kvs->values[slot] = *value;
}

HM_DEFINE_FULL(
    server_client_hm,
    hash32,
    const struct net_addr*,
    struct server_client,
    16,
    server_client_hm_kvs_hash,
    server_client_hm_kvs_alloc,
    server_client_hm_kvs_free_old,
    server_client_hm_kvs_free,
    server_client_hm_kvs_get_key,
    server_client_hm_kvs_set_key,
    server_client_hm_kvs_keys_equal,
    server_client_hm_kvs_get_value,
    server_client_hm_kvs_set_value,
    16,
    70)
