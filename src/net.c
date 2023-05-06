#include "clither/log.h"
#include "clither/net.h"
#include "clither/protocol.h"

#include "clither/q.h"

#include <string.h>
#include <assert.h>

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <winsock2.h>
#   include <ws2ipdef.h>
#   include <WS2tcpip.h>
#else
#   include <errno.h>
#   include <unistd.h>
#   include <netdb.h>
#   include <sys/socket.h>
#   include <arpa/inet.h>
#   include <ifaddrs.h>
#endif

/*
 * 576 = minimum maximum reassembly buffer size
 * 60  = maximum IP header size
 * 8   = UDP header size
 */
#define MAX_UDP_PACKET_SIZE (576 - 60 - 8)

struct client_table_entry
{
    struct cs_vector pending_reliable;
    socklen_t addrlen;
};

#define CLIENT_TABLE_FOR_EACH(table, k, v) \
    HASHMAP_FOR_EACH(table, struct sockaddr, struct client_table_entry, k, v)
#define CLIENT_TABLE_END_EACH \
    HASHMAP_END_EACH

/* ------------------------------------------------------------------------- */
static void
sockaddr_to_str(char* ipstr, const struct sockaddr* addr, int maxlen)
{
    if (addr->sa_family == AF_INET)
        inet_ntop(addr->sa_family, &((struct sockaddr_in*)addr)->sin_addr, ipstr, maxlen);
    else if (addr->sa_family == AF_INET6)
        inet_ntop(addr->sa_family, &((struct sockaddr_in6*)addr)->sin6_addr, ipstr, maxlen);
    else
        ipstr[0] = '\0';
}

/* ------------------------------------------------------------------------- */
static cs_hash32
hash32_sockaddr(const void* addr_storage, uintptr_t len)
{
    const struct sockaddr* addr = addr_storage;
    if (addr->sa_family == AF_INET)
        /* IPv4 is a uint32_t */
        return (cs_hash32)((struct sockaddr_in*)addr)->sin_addr.s_addr;
    else if (addr->sa_family == AF_INET6)
        /* IPv6 is 16 bytes (128 bits) */
        return hash32_jenkins_oaat(((struct sockaddr_in6*)addr)->sin6_addr.s6_addr, 16);
    else
        assert(0);

    (void)len;
}

/* ------------------------------------------------------------------------- */
int
net_init(void)
{
#if defined(_WIN32)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        log_err("WSAStartup failed\n");
        return -1;
    }

    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        log_err("Version 2.2 of Winsock is not available\n");
        WSACleanup();
        return -1;
    }

    return 0;
#else
    return 0;
#endif
}

/* ------------------------------------------------------------------------- */
void
net_deinit(void)
{
#if defined(_WIN32)
    WSACleanup();
#endif
}

/* ------------------------------------------------------------------------- */
void
net_log_host_ips(void)
{
#if defined(_WIN32)
    log_note("Your local IP address is: (todo)\n");
    log_note("Your external IP address is: (todo)\n");
#else
    struct ifaddrs* ifaddr;
    struct ifaddrs* p;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) < 0)
    {
        log_warn("Failed to get host IP using getifaddrs()\n");
        return;
    }

    for (p = ifaddr; p; p = p->ifa_next)
    {
        if (p->ifa_addr == NULL)
            continue;

        s = getnameinfo(p->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    }

    freeifaddrs(ifaddr);
#endif
}

/* ------------------------------------------------------------------------- */
int
server_init(struct server* server, const char* bind_address, const char* port)
{
    struct addrinfo hints;
    struct addrinfo* candidates;
    struct addrinfo* p;
    char ipstr[INET6_ADDRSTRLEN];
    int ret;

    /*
     * Set up hints structure. If an IP address was specified in the command
     * line args, then we call getaddrinfo() with that info. If not, then we
     * have to set AI_PASSIVE and call getaddrinfo() with NULL as the address
     */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM;  /* UDP */
    if (*bind_address)
        ret = getaddrinfo(bind_address, port, &hints, &candidates);
    else
    {
        hints.ai_flags = AI_PASSIVE;
        ret = getaddrinfo(NULL, port, &hints, &candidates);
    }
    if (ret != 0)
    {
        log_err("getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }

    for (p = candidates; p != NULL; p = p->ai_next)
    {
        sockaddr_to_str(ipstr, p->ai_addr, sizeof ipstr);

        log_dbg("Attempting to bind UDP %s:%s...\n", ipstr, port);
        server->udp_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (server->udp_sock == -1)
            continue;

        if (bind(server->udp_sock, p->ai_addr, p->ai_addrlen) == 0)
            break;
        log_warn("bind() failed for UDP %s:%s: %s\n", ipstr, port, strerror(errno));

        close(server->udp_sock);
    }
    freeaddrinfo(candidates);
    if (p == NULL)
    {
        log_err("Failed to bind UDP socket\n");
        close(server->udp_sock);
        return -1;
    }

    log_dbg("Bound UDP socket to %s:%s\n", ipstr, port);

    hashmap_init_with_options(
        &server->client_table,
        sizeof(struct sockaddr_in6),  /* Assumption is: ipv6 is the largest address we can receive */
        sizeof(struct client_table_entry),
        128,
        hash32_sockaddr);

    return 0;
}

/* ------------------------------------------------------------------------- */
void
server_deinit(struct server* server)
{
    log_dbg("Closing socket\n");
#if defined(_WIN32)
    closesocket(server->udp_sock);
#else
    close(server->udp_sock);
#endif

    CLIENT_TABLE_FOR_EACH(&server->client_table, addr, client)
        net_msg_queue_deinit(&client->pending_reliable);
    CLIENT_TABLE_END_EACH
    hashmap_deinit(&server->client_table);
}

/* ------------------------------------------------------------------------- */
void
server_send_pending_data(struct server* server)
{
    char buf[MAX_UDP_PACKET_SIZE];

    CLIENT_TABLE_FOR_EACH(&server->client_table, addr, client)
        uint8_t type;
        int len = 0;

        NET_MSG_FOR_EACH(&client->pending_reliable, msg)
            if (len + msg->payload_len + 2 > MAX_UDP_PACKET_SIZE)
                continue;

            type = (uint8_t)msg->type;
            memcpy(buf + 0, &type, 1);
            memcpy(buf + 1, &msg->payload_len, 1);
            memcpy(buf + 2, msg->payload, msg->payload_len);
            len += msg->payload_len + 2;
        NET_MSG_END_EACH

        log_dbg("Sending UDP packet size=%d\n", len);
        sendto(server->udp_sock, buf, len, 0, (struct sockaddr*)addr, client->addrlen);
    CLIENT_TABLE_END_EACH
}

/* ------------------------------------------------------------------------- */
int
server_recv(struct server* server)
{
    char buf[MAX_UDP_PACKET_SIZE];
    char ipstr[INET6_ADDRSTRLEN];
    struct sockaddr_in6 client_addr;  /* Assumption is: IPv6 is the largest address possible */
    socklen_t client_addr_len;
    int bytes_received;
    int i;

    memset(&client_addr, 0, sizeof client_addr);
    client_addr_len = sizeof(client_addr);
    bytes_received = recvfrom(server->udp_sock, buf, MAX_UDP_PACKET_SIZE, MSG_DONTWAIT, (struct sockaddr*)&client_addr, &client_addr_len);
    if (bytes_received < 0)
    {
        if (errno == EAGAIN)
            return 0;

        log_err("Receive call failed: %s\n", strerror(errno));
        return -1;
    }

    for (i = 0; i < bytes_received - 1;)
    {
        struct client_table_entry* client;
        enum net_msg_type type = buf[i+0];
        uint8_t payload_len = buf[i+1];
        if (i + payload_len + 2 > bytes_received)
        {
            sockaddr_to_str(ipstr, (struct sockaddr*)&client_addr, sizeof *ipstr);
            log_warn("Invalid payload length \"%d\" received from client %s\n", (int)payload_len, ipstr);
            log_warn("Dropping rest of packet\n");
            return 0;
        }

        client = hashmap_find(&server->client_table, &client_addr);
        if (client == NULL && type != MSG_JOIN)
        {
            sockaddr_to_str(ipstr, (struct sockaddr*)&client_addr, sizeof *ipstr);
            log_warn("Received packet from unknown client %s\n", ipstr);
            return 0;
        }

        switch (type)
        {
            case MSG_JOIN: {
                char name[128];
                struct qpos2 spawn_pos;
                if (client != NULL)
                    break;

                /* TODO: Create snake in world */

                memcpy(name, buf+2, payload_len);
                name[payload_len] = '\0';
                log_dbg("Protocol: MSG_JOIN <- \"%s\"\n", name);

                client = hashmap_emplace(&server->client_table, &client_addr);
                client->addrlen = client_addr_len;
                net_msg_queue_init(&client->pending_reliable);

                protocol_join_game_response(&client->pending_reliable, &spawn_pos);
            } break;

            case MSG_JOIN_ACK: {

            } break;
        }

        i += payload_len + 2;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
int
client_init(struct client* client, const char* server_address, const char* port)
{
    struct addrinfo hints;
    struct addrinfo* candidates;
    struct addrinfo* p;
    char ipstr[INET6_ADDRSTRLEN];
    int ret;

    if (!*server_address)
    {
        log_err("No server IP address was specified! Can't init client socket\n");
        log_err("You can use --ip <address> to specify an address to connect to\n");
        return -1;
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM;  /* UDP */

    if ((ret = getaddrinfo(server_address, port, &hints, &candidates)) != 0)
    {
        log_err("getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }
    for (p = candidates; p != NULL; p = p->ai_next)
    {
        sockaddr_to_str(ipstr, p->ai_addr, sizeof ipstr);

        log_dbg("Attempting to connect UDP socket %s:%s...\n", ipstr, port);
        client->udp_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (client->udp_sock == -1)
            continue;

        /*
         * When connecting a UDP socket, we can use send() instead of sendto()
         * This way, the server address (socketaddr_storage) doens't need to be
         * saved in the client structure
         */
        if (connect(client->udp_sock, p->ai_addr, p->ai_addrlen) == 0)
            break;
        log_warn("connect() failed for UDP %s:%s: %s\n", ipstr, port, strerror(errno));

        close(client->udp_sock);
    }
    freeaddrinfo(candidates);
    if (p == NULL)
    {
        log_err("Failed to connect UDP socket\n");
        close(client->udp_sock);
        return -1;
    }

    log_dbg("Connected UDP socket to %s:%s\n", ipstr, port);

    net_msg_queue_init(&client->pending_reliable);

    return 0;
}

/* ------------------------------------------------------------------------- */
void
client_deinit(struct client* client)
{
    log_dbg("Closing socket\n");
#if defined(_WIN32)
    closesocket(client->udp_sock);
#else
    close(client->udp_sock);
#endif

    net_msg_queue_deinit(&client->pending_reliable);
}

/* ------------------------------------------------------------------------- */
void
client_send_pending_data(struct client* client)
{
    uint8_t type;
    char buf[MAX_UDP_PACKET_SIZE];
    int len = 0;
    NET_MSG_FOR_EACH(&client->pending_reliable, msg)
        if (len + msg->payload_len + 2 > MAX_UDP_PACKET_SIZE)
            continue;

        type = (uint8_t)msg->type;
        memcpy(buf + 0, &type, 1);
        memcpy(buf + 1, &msg->payload_len, 1);
        memcpy(buf + 2, msg->payload, msg->payload_len);
        len += msg->payload_len + 2;
    NET_MSG_END_EACH

    log_dbg("Sending UDP packet size=%d\n", len);
    send(client->udp_sock, buf, len, 0);
}

/* ------------------------------------------------------------------------- */
int
client_recv(struct client* client)
{
    char buf[MAX_UDP_PACKET_SIZE];
    char ipstr[INET6_ADDRSTRLEN];
    struct sockaddr_in6 server_addr;  /* Assumption is: IPv6 is the largest address possible */
    socklen_t server_addr_len = sizeof(server_addr);
    int bytes_received;
    int i;

    memset(&server_addr, 0, sizeof server_addr);
    server_addr_len = sizeof(server_addr);
    bytes_received = recvfrom(client->udp_sock, buf, MAX_UDP_PACKET_SIZE, MSG_DONTWAIT, (struct sockaddr*)&server_addr, &server_addr_len);
    if (bytes_received < 0)
    {
        if (errno == EAGAIN)
            return 0;

        log_err("Receive call failed: %s\n", strerror(errno));
        return -1;
    }

    for (i = 0; i < bytes_received - 1;)
    {
        enum net_msg_type type = buf[i+0];
        uint8_t payload_len = buf[i+1];
        if (i + payload_len + 2 > bytes_received)
        {
            sockaddr_to_str(ipstr, (struct sockaddr*)&server_addr, sizeof *ipstr);
            log_warn("Invalid payload length \"%d\" received from %s\n", (int)payload_len, ipstr);
            log_warn("Dropping rest of packet\n");
            return 0;
        }

        switch (type)
        {
            case MSG_JOIN: {

            } break;

            case MSG_JOIN_ACK:
                break;
        }
    }

    return 0;
}
