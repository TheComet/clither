#pragma once

#include "clither/config.h"

struct args;

struct net_sockets
{
    int tcp;
    int udp;
};

int
net_bind_server_sockets(struct net_sockets* sockets, const struct args* a);

void
net_close_sockets(struct net_sockets* sockets);
