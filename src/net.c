#include "clither/args.h"
#include "clither/log.h"
#include "clither/net.h"

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
#endif

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
net_bind_server_sockets(struct net_sockets* sockets, const struct args* a)
{
    struct addrinfo hints;
    struct addrinfo* candidates;
    struct addrinfo* p;
    const char* addr;
    char ipstr[INET6_ADDRSTRLEN];
    int ret;

    /*
     * Set up hints structure. If an IP address was specified in the command
     * line args, then we call getaddrinfo() with that info. If not, then we
     * have to set AI_PASSIVE and call getaddrinfo() with NULL as the address
     */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     /* IPv4 or IPv6 */
    if (*a->ip)
        addr = a->ip;
    else
    {
        addr = NULL;
        hints.ai_flags = AI_PASSIVE;
    }

    /* TCP candidates */
    hints.ai_socktype = SOCK_STREAM;
    if ((ret = getaddrinfo(addr, a->port, &hints, &candidates)) != 0)
    {
        log_err("getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }
    for (p = candidates; p != NULL; p = p->ai_next)
    {
        get_addrinfo_ip_address_str(ipstr, p, sizeof ipstr);

        log_dbg("Attempting to bind TCP %s:%s...\n", ipstr, a->port);
        sockets->tcp = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockets->tcp == -1)
            continue;

        if (bind(sockets->tcp, p->ai_addr, p->ai_addrlen) == 0)
            break;
        log_warn("bind() failed for TCP %s:%s: %s\n", ipstr, a->port, strerror(errno));

        close(sockets->tcp);
    }
    freeaddrinfo(candidates);
    if (p == NULL)
    {
        log_err("Failed to bind TCP socket\n");
        return -1;
    }

    /* UDP candidates */
    hints.ai_socktype = SOCK_DGRAM;
    if ((ret = getaddrinfo(addr, a->port, &hints, &candidates)) != 0)
    {
        log_err("getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }
    for (p = candidates; p != NULL; p = p->ai_next)
    {
        get_addrinfo_ip_address_str(ipstr, p, sizeof ipstr);

        log_dbg("Attempting to bind UDP %s:%s...\n", ipstr, a->port);
        sockets->udp = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockets->udp == -1)
            continue;

        if (bind(sockets->udp, p->ai_addr, p->ai_addrlen) == 0)
            break;
        log_warn("bind() failed for UDP %s:%s: %s\n", ipstr, a->port, strerror(errno));

        close(sockets->udp);
    }
    freeaddrinfo(candidates);
    if (p == NULL)
    {
        log_err("Failed to bind TCP socket\n");
        close(sockets->tcp);
        return -1;
    }

    log_info("Bound TCP and UDP sockets to %s:%s\n", ipstr, a->port);

    return 0;
}

/* ------------------------------------------------------------------------- */
void
net_close_sockets(struct net_sockets* sockets)
{
    log_dbg("Closing sockets\n");
#if defined(_WIN32)
    closesocket(sockets->tcp);
    closesocket(sockets->udp);
#else
    close(sockets->tcp);
    close(sockets->udp);
#endif
}
