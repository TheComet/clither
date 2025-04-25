#define WIN32_LEAN_AND_MEAN

#include "clither/platform/net.h"
#include "clither/util/log.h"
#include <windows.h>
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

VEC_DEFINE(sockfd_vec, int, 8)

/* ------------------------------------------------------------------------- */
int net_set_nonblock_reuse(int sockfd)
{
    unsigned long nonblock = 1;
    int           enable = 1;
    if (ioctlsocket(sockfd, FIONBIO, &nonblock) != 0)
        return log_err(
            "ioctlsocket() failed for socket: %d\n", WSAGetLastError());

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        return log_err(
            "sotsocketopt(SO_REUSEADDR) failed for socket: %d\n",
            WSAGetLastError());
#if defined(SO_REUSEPORT)
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0)
        return log_err(
            "sotsocketopt(SO_REUSEPORT) failed for socket: %d\n",
            WSAGetLastError());
#endif

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
    WSADATA wsaData;
    CLITHER_STATIC_ASSERT(sizeof(struct sockaddr_in) <= NET_MAX_ADDRLEN);
    CLITHER_STATIC_ASSERT(sizeof(struct sockaddr_in6) <= NET_MAX_ADDRLEN);

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
}

/* ------------------------------------------------------------------------- */
void net_deinit(void)
{
    WSACleanup();
}

/* ------------------------------------------------------------------------- */
void net_log_host_ips(void)
{
    log_note("Your local IP address is: (todo)\n");
    log_note("Your external IP address is: (todo)\n");
}

/* ------------------------------------------------------------------------- */
static int net_bind(const char* bind_address, const char* port, int socktype)
{
    struct addrinfo     hints;
    struct addrinfo*    candidates;
    struct addrinfo*    p;
    struct net_addr_str ipstr;
    int                 ret;
    int                 sockfd = -1;

    /*
     * Set up hints structure. If an IP address was specified in the command
     * line args, then we call getaddrinfo() with that info. If not, then we
     * have to set AI_PASSIVE and call getaddrinfo() with NULL as the address
     */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
    hints.ai_socktype = socktype;
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
        log_dbg(
            "Attempting to bind %s %s:%s\n",
            socktype == SOCK_DGRAM ? "UDP" : "TCP",
            ipstr.cstr,
            port);
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
            continue;

        /* We want non-blocking sockets */
        if (net_set_nonblock_reuse(sockfd) < 0)
        {
            closesocket(sockfd);
            continue;
        }

        if (bind(sockfd, p->ai_addr, (int)p->ai_addrlen) != 0)
        {
            log_warn(
                "bind() failed for %s %s:%s: %d\n",
                socktype == SOCK_DGRAM ? "UDP" : "TCP",
                ipstr.cstr,
                port,
                WSAGetLastError());
            closesocket(sockfd);
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

    log_dbg(
        "Bound %s socket to %s:%s\n",
        socktype == SOCK_DGRAM ? "UDP" : "TCP",
        ipstr.cstr,
        port);
    return sockfd;
}

/* ------------------------------------------------------------------------- */
int net_host_udp(const char* bind_address, const char* port)
{
    return net_bind(bind_address, port, SOCK_DGRAM);
}

/* ------------------------------------------------------------------------- */
static int net_connect(
    struct sockfd_vec** sockfds,
    const char*         server_address,
    const char*         port,
    int                 socktype)
{
    struct addrinfo     hints;
    struct addrinfo*    candidates;
    struct addrinfo*    p;
    struct net_addr_str ipstr;
    int                 ret;
    int                 sockfd = -1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
    hints.ai_socktype = socktype;

    if ((ret = getaddrinfo(server_address, port, &hints, &candidates)) != 0)
    {
        log_err("getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }
    for (p = candidates; p != NULL; p = p->ai_next)
    {
        ai_addr_to_str(&ipstr, p->ai_addr);

        log_dbg(
            "Attempting to connect %s socket %s:%s...\n",
            socktype == SOCK_DGRAM ? "UDP" : "TCP",
            ipstr.cstr,
            port);
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
            continue;

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
            closesocket(sockfd);
            continue;
        }

        /* We want non-blocking sockets: NOTE: This needs to be done after
         * connect(), otherwise connect() will return EINPROGRESS */
        if (net_set_nonblock_reuse(sockfd) < 0)
        {
            close(sockfd);
            continue;
        }

        log_dbg(
            "Connected %s socket to %s:%s\n",
            socktype == SOCK_DGRAM ? "UDP" : "TCP",
            ipstr.cstr,
            port);
        sockfd_vec_push(sockfds, sockfd);
    }
    freeaddrinfo(candidates);

    if (vec_count(*sockfds) == 0)
    {
        log_err(
            "Failed to connect any %s socket\n",
            socktype == SOCK_DGRAM ? "UDP" : "TCP");
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
int net_connect_udp(
    struct sockfd_vec** sockfds, const char* server_address, const char* port)
{
    return net_connect(sockfds, server_address, port, SOCK_DGRAM);
}
int net_connect_tcp(
    struct sockfd_vec** sockfds, const char* server_address, const char* port)
{
    return net_connect(sockfds, server_address, port, SOCK_STREAM);
}

/* ------------------------------------------------------------------------- */
void net_close(int sockfd)
{
#if defined(CLITHER_LOG_DEBUG)
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
    closesocket(sockfd);
}

/* ------------------------------------------------------------------------- */
int net_sendto(
    int sockfd, const struct net_addr* addr, const void* buf, int len)
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
int net_recvfrom(int sockfd, struct net_addr* addr, void* buf, int capacity)
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
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return 0;
        log_err("Receive call failed: %d\n", WSAGetLastError());
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
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return 0;
        log_err("Receive call failed: %d\n", WSAGetLastError());
        return -1;
    }

    return bytes_received;
}

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_SERVER_WEBSOCKETS)
int net_host_tcp(const char* bind_address, const char* port)
{
    int fd = net_bind(bind_address, port, SOCK_STREAM);
    if (fd < 0)
        return fd;
    if (listen(fd, 1) < 0)
    {
        log_err("listen() failed: %d\n", WSAGetLastError());
        close(fd);
        return -1;
    }
    return fd;
}
int net_accept(int sockfd, struct net_addr* addr)
{
    socklen_t addrlen = sizeof(addr->sockaddr_storage);
    int       fd =
        accept(sockfd, (struct sockaddr*)&addr->sockaddr_storage, &addrlen);
    if (fd < 0)
    {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return 0;
        log_err("accept() failed: %d\n", WSAGetLastError());
        return -1;
    }
    addr->len = (int)addrlen;
    return fd;
}
#endif
