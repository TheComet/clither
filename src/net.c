#include "clither/log.h"
#include "clither/net.h"
#include "clither/protocol.h"

#include <string.h>

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <winsock2.h>
#   include <ws2ipdef.h>
#   include <WS2tcpip.h>
#else
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif

/* 
 * 576 = minimum maximum reassembly buffer size
 * 60  = maximum IP header size
 * 8   = UDP header size
 */
#define MAX_UDP_PACKET_SIZE (576 - 60 - 8)

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

    for (p = ifaddr; *p; p = p->ifa_next)
    {
        if (p->ifa_addr == NULL)
            continue;

        s = getnameinfo(p->ifa_addr, sizeof(sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    }

    freeifaddrs(ifaddr);
#endif
}

/* ------------------------------------------------------------------------- */
static void get_addrinfo_ip_address_str(char* ipstr, const struct addrinfo* info, int maxlen)
{
    void* addr = NULL;
    if (info->ai_family == AF_INET)
        addr = &((struct sockaddr_in*)info->ai_addr)->sin_addr;
    else if (info->ai_family == AF_INET6)
        addr = &((struct sockaddr_in6*)info->ai_addr)->sin6_addr;

    if (addr)
        inet_ntop(info->ai_family, addr, ipstr, maxlen);
    else
        ipstr[0] = '\0';
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
        get_addrinfo_ip_address_str(ipstr, p, sizeof ipstr);

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

    net_msg_queue_init(&server->pending_reliable);

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
    close(conn->udp_sock);
#endif

    net_msg_queue_deinit(&server->pending_reliable);
}

/* ------------------------------------------------------------------------- */
void
server_send_pending_data(struct server* server)
{

}

/* ------------------------------------------------------------------------- */
void
server_recv(struct server* server)
{
    char buf[MAX_UDP_PACKET_SIZE];
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;
    int len = recvfrom(server->udp_sock, buf, MAX_UDP_PACKET_SIZE, 0, (struct sockaddr*)&client_addr, &client_addr_len);
    if (len > 0)
    {
        log_dbg("Received data\n");
    }
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
        get_addrinfo_ip_address_str(ipstr, p, sizeof ipstr);

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

}
