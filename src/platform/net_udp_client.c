#include "clither/platform/net.h"

struct net_connection
{
    struct sockfd_vec* sockets;
    int                timeout_counter;
};

static struct net_connection*
udp_client_create(const char* address, const char* port)
{
    struct net_connection* client = mem_alloc(sizeof(*client));
    if (!client)
        goto alloc_client_failed;

    sockfd_vec_init(&client->sockets);
    if (net_connect_udp(&client->sockets, address, port) != 0)
        goto connect_failed;

    return client;

connect_failed:
    mem_free(client);
alloc_client_failed:
    return NULL;
}

static void udp_client_destroy(struct net_connection* client)
{
    int* fd;
    vec_for_each (client->sockets, fd)
        net_close(*fd);
    sockfd_vec_deinit(client->sockets);

    mem_free(client);
}

static enum net_receive_result
udp_client_receive(struct net_connection* client, struct net_packet* packet)
{
    while (vec_count(client->sockets) > 0)
    {
        int* sockfd = vec_last(client->sockets);
        packet->len = net_recv(*sockfd, packet->data, sizeof(packet->data));
        if (packet->len == 0)
            return NET_RECEIVE_NO_DATA;
        if (packet->len > 0)
            return NET_RECEIVE_DATA;

        net_close(*sockfd_vec_pop(client->sockets));
    }

    return log_err("Connection to server was closed\n");
}

static int
udp_client_send(struct net_connection* client, const struct net_packet* packet)
{
    /*
     * The client was initialized with a list of possible sockets. This is
     * because we can't know without first communicating with the server which
     * protocol (IPv4 vs IPv6) is being used. If a send call fails, and there
     * are more sockets, simply close the one that failed and try the next one.
     */
    while (vec_count(client->sockets) > 0)
    {
        int* sockfd = vec_last(client->sockets);
        if (net_send(*sockfd, packet->data, packet->len) >= 0)
            return 0;

        net_close(*sockfd_vec_pop(client->sockets));
    }

    return log_err("Connection to server was closed\n");
}

const struct net_client_interface net_udp_client = {
    udp_client_create, udp_client_destroy, udp_client_receive, udp_client_send};
