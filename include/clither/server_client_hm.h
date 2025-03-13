#pragma once

#include "clither/hm.h"
#include "clither/server_client.h"

struct net_addr;

struct server_client_hm_kvs
{
    struct net_addr*      keys;
    struct server_client* values;
};

HM_DECLARE_FULL(
    server_client_hm,
    hash32,
    const struct net_addr*,
    struct server_client,
    16,
    struct server_client_hm_kvs)

const struct net_addr* server_client_hm_kvs_get_key(
    const struct server_client_hm_kvs* kvs, int16_t slot);

struct server_client* server_client_hm_kvs_get_value(
    const struct server_client_hm_kvs* kvs, int16_t slot);

#define server_client_hm_for_each(server_clients, slot, addr, client)          \
    hm_for_each_full (                                                         \
        server_clients,                                                        \
        slot,                                                                  \
        addr,                                                                  \
        client,                                                                \
        server_client_hm_kvs_get_key,                                          \
        server_client_hm_kvs_get_value)
