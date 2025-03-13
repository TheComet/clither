#pragma once

#include "clither/hm.h"

struct net_addr_hm_kvs
{
    struct net_addr* keys;
    int*             values;
};

HM_DECLARE_FULL(
    net_addr_hm,
    hash32,
    const struct net_addr*,
    int,
    16,
    struct net_addr_hm_kvs)

const struct net_addr*
net_addr_hm_kvs_get_key(const struct net_addr_hm_kvs* kvs, int16_t slot);

int* net_addr_hm_kvs_get_value(const struct net_addr_hm_kvs* kvs, int16_t slot);

#define net_addr_hm_for_each(server_clients, slot, addr, client)               \
    hm_for_each_full (                                                         \
        server_clients,                                                        \
        slot,                                                                  \
        addr,                                                                  \
        client,                                                                \
        net_addr_hm_kvs_get_key,                                               \
        net_addr_hm_kvs_get_value)
