#pragma once

#include "clither/util/hmap.h"

struct net_addr_hmap_kvs
{
    struct net_addr* keys;
    int*             values;
};

HMAP_DECLARE_FULL(
    CLITHER_PRIVATE_API,
    net_addr_hmap,
    hash32,
    const struct net_addr*,
    int,
    16,
    struct net_addr_hmap_kvs)

const struct net_addr*
net_addr_hmap_kvs_get_key(const struct net_addr_hmap_kvs* kvs, int16_t slot);

int* net_addr_hmap_kvs_get_value(
    const struct net_addr_hmap_kvs* kvs, int16_t slot);

#define net_addr_hmap_for_each(server_clients, slot, addr, client)             \
    hmap_for_each_full (                                                       \
        server_clients,                                                        \
        slot,                                                                  \
        addr,                                                                  \
        client,                                                                \
        net_addr_hmap_kvs_get_key,                                             \
        net_addr_hmap_kvs_get_value)
