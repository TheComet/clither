#include "clither/log.h"
#include "clither/mem.h"
#include "clither/net.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

VEC_DEFINE(sockfd_vec, int, 8)

CLITHER_STATIC_ASSERT(sizeof(struct sockaddr_in) <= NET_MAX_ADDRLEN);
CLITHER_STATIC_ASSERT(sizeof(struct sockaddr_in6) <= NET_MAX_ADDRLEN);

/* ------------------------------------------------------------------------- */
static int set_nonblocking(int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1)
    {
        log_err("fcntl() failed for socket: %s\n", strerror(errno));
        return -1;
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        log_err("fcntl() failed for socket: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
static void ai_addr_to_str(struct net_addr_str* str, const struct sockaddr* a)
{
    switch (a->sa_family)
    {
        case AF_INET:
            inet_ntop(
                a->sa_family,
                &((const struct sockaddr_in*)a)->sin_addr,
                str->cstr,
                sizeof(str->cstr));
            break;
        case AF_INET6:
            inet_ntop(
                a->sa_family,
                &((const struct sockaddr_in6*)a)->sin6_addr,
                str->cstr,
                sizeof(str->cstr));
            break;
        default: strcpy(str->cstr, "(unknown)");
    }
}
void net_addr_to_str(struct net_addr_str* str, const struct net_addr* addr)
{
    const struct sockaddr* a = (const struct sockaddr*)addr->sockaddr_storage;
    ai_addr_to_str(str, a);
}

/* ------------------------------------------------------------------------- */
int net_init(void)
{
    return 0;
}

/* ------------------------------------------------------------------------- */
void net_deinit(void)
{
}

/* ------------------------------------------------------------------------- */
void net_log_host_ips(void)
{
    struct ifaddrs* ifaddr;
    struct ifaddrs* p;

    if (getifaddrs(&ifaddr) < 0)
    {
        log_warn("Failed to get host IP using getifaddrs()\n");
        return;
    }

    for (p = ifaddr; p; p = p->ifa_next)
    {
        if (p->ifa_addr == NULL)
            continue;

        /*s = getnameinfo(p->ifa_addr, sizeof(struct sockaddr_in), host,
         * NI_MAXHOST, NULL, 0, NI_NUMERICHOST);*/
    }

    freeifaddrs(ifaddr);
}

/* ------------------------------------------------------------------------- */
int net_bind(const char* bind_address, const char* port)
{
    struct addrinfo     hints;
    struct addrinfo*    candidates;
    struct addrinfo*    p;
    struct net_addr_str ipstr;
    int                 ret;
    int                 sockfd;

    /*
     * Set up hints structure. If an IP address was specified in the command
     * line args, then we call getaddrinfo() with that info. If not, then we
     * have to set AI_PASSIVE and call getaddrinfo() with NULL as the address
     */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;    /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* UDP */
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
        ai_addr_to_str(&ipstr, p->ai_addr);
        log_dbg("Attempting to bind UDP %s:%s\n", ipstr.cstr, port);
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
            continue;

        /* We want non-blocking sockets */
        if (set_nonblocking(sockfd) < 0)
        {
            close(sockfd);
            continue;
        }

        if (bind(sockfd, p->ai_addr, (int)p->ai_addrlen) != 0)
        {
            log_warn(
                "bind() failed for UDP %s:%s: %s\n",
                ipstr.cstr,
                port,
                strerror(errno));
            close(sockfd);
            continue;
        }
        break;
    }
    freeaddrinfo(candidates);
    if (p == NULL)
    {
        log_err("Failed to bind UDP socket\n");
        return -1;
    }

    log_dbg("Bound UDP socket to %s:%s\n", ipstr.cstr, port);
    return sockfd;
}

/* ------------------------------------------------------------------------- */
int net_connect(
    struct sockfd_vec** sockfds, const char* server_address, const char* port)
{
    struct addrinfo     hints;
    struct addrinfo*    candidates;
    struct addrinfo*    p;
    struct net_addr_str ipstr;
    int                 ret;
    int                 sockfd;

    CLITHER_DEBUG_ASSERT(vec_count(*sockfds) == 0);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;    /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* UDP */

    if ((ret = getaddrinfo(server_address, port, &hints, &candidates)) != 0)
    {
        log_err("getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }
    for (p = candidates; p != NULL; p = p->ai_next)
    {
        ai_addr_to_str(&ipstr, p->ai_addr);

        log_dbg(
            "Attempting to connect UDP socket %s:%s...\n", ipstr.cstr, port);
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
            continue;

        /* We want non-blocking sockets */
        if (set_nonblocking(sockfd) < 0)
        {
            close(sockfd);
            continue;
        }

        /*
         * When connecting a UDP socket, we can use send() instead of sendto()
         * This way, the server address (socketaddr_storage) doens't need to be
         * saved in the client structure
         */
        if (connect(sockfd, p->ai_addr, (int)p->ai_addrlen) != 0)
        {
            log_warn(
                "connect() failed for UDP %s:%s: %s\n",
                ipstr.cstr,
                port,
                strerror(errno));
            close(sockfd);
            continue;
        }

        log_dbg("Connected UDP socket to %s:%s\n", ipstr.cstr, port);
        sockfd_vec_push(sockfds, sockfd);
    }
    freeaddrinfo(candidates);

    if (vec_count(*sockfds) == 0)
    {
        log_err("Failed to connect any UDP socket\n");
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
void net_close(int sockfd)
{
#if defined(CLITHER_DEBUG)
    struct sockaddr     addr;
    struct net_addr_str ipstr;
    socklen_t           addr_len = sizeof(addr);
    getsockname(sockfd, &addr, &addr_len);
    ai_addr_to_str(&ipstr, &addr);
    log_dbg(
        "Closing socket %s:%d\n",
        ipstr.cstr,
        addr.sa_family == AF_INET
            ? ntohs(((struct sockaddr_in*)&addr)->sin_port)
        : addr.sa_family == AF_INET6
            ? ntohs(((struct sockaddr_in6*)&addr)->sin6_port)
            : 0);
#endif
    close(sockfd);
}

/* ------------------------------------------------------------------------- */
int net_sendto(
    int sockfd, const void* buf, int len, const struct net_addr* addr)
{
    const struct sockaddr* sockaddr =
        (const struct sockaddr*)addr->sockaddr_storage;
    return sendto(sockfd, buf, len, 0, sockaddr, addr->len);
}

/* ------------------------------------------------------------------------- */
int net_send(int sockfd, const void* buf, int len)
{
    return send(sockfd, buf, len, 0);
}

/* ------------------------------------------------------------------------- */
int net_recvfrom(int sockfd, void* buf, int capacity, struct net_addr* addr)
{
    socklen_t addrlen_received = sizeof(addr->sockaddr_storage);

    int bytes_received = recvfrom(
        sockfd,
        buf,
        capacity,
        0,
        (struct sockaddr*)&addr->sockaddr_storage,
        &addrlen_received);
    addr->len = (int)addrlen_received;

    if (bytes_received < 0)
    {
        if (errno == EAGAIN)
            return 0;
        log_err("Receive call failed: %s\n", strerror(errno));
        return -1;
    }

    return bytes_received;
}

/* ------------------------------------------------------------------------- */
int net_recv(int sockfd, void* buf, int capacity)
{
    int bytes_received = recv(sockfd, buf, capacity, 0);

    if (bytes_received < 0)
    {
        if (errno == EAGAIN)
            return 0;
        log_err("Receive call failed: %s\n", strerror(errno));
        return -1;
    }

    return bytes_received;
}
