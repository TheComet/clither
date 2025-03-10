#include "clither/net.h"
#include "clither/net_addr_hm.h"
#include "clither/server_client.h"

static int net_addr_hm_kvs_alloc(
    struct net_addr_hm_kvs* kvs,
    struct net_addr_hm_kvs* old_kvs,
    int16_t                 capacity)
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

static void net_addr_hm_kvs_free(struct net_addr_hm_kvs* kvs)
{
    mem_free(kvs->values);
    mem_free(kvs->keys);
}

static void net_addr_hm_kvs_free_old(struct net_addr_hm_kvs* kvs)
{
    net_addr_hm_kvs_free(kvs);
}

static hash32 net_addr_hm_kvs_hash(const struct net_addr* key)
{
    return hash32_jenkins_oaat(key->sockaddr_storage, key->len);
}

const struct net_addr*
net_addr_hm_kvs_get_key(const struct net_addr_hm_kvs* kvs, int16_t slot)
{
    return &kvs->keys[slot];
}

static void net_addr_hm_kvs_set_key(
    struct net_addr_hm_kvs* kvs, int16_t slot, const struct net_addr* key)
{
    kvs->keys[slot].len = key->len;
    memcpy(kvs->keys[slot].sockaddr_storage, key->sockaddr_storage, key->len);
}

static int
net_addr_hm_kvs_keys_equal(const struct net_addr* k1, const struct net_addr* k2)
{
    return k1->len == k2->len && memcmp(k1, k2, k1->len) == 0;
}

int* net_addr_hm_kvs_get_value(const struct net_addr_hm_kvs* kvs, int16_t slot)
{
    return &kvs->values[slot];
}

static void net_addr_hm_kvs_set_value(
    struct net_addr_hm_kvs* kvs, int16_t slot, const int* value)
{
    kvs->values[slot] = *value;
}

HM_DEFINE_FULL(
    net_addr_hm,
    hash32,
    const struct net_addr*,
    int,
    16,
    net_addr_hm_kvs_hash,
    net_addr_hm_kvs_alloc,
    net_addr_hm_kvs_free_old,
    net_addr_hm_kvs_free,
    net_addr_hm_kvs_get_key,
    net_addr_hm_kvs_set_key,
    net_addr_hm_kvs_keys_equal,
    net_addr_hm_kvs_get_value,
    net_addr_hm_kvs_set_value,
    128,
    70)
