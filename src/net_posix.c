#include "clither/log.h"
#include "clither/net.h"

#include "cstructures/vector.h"

#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

/* ------------------------------------------------------------------------- */
static int
set_nonblocking(int sockfd)
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
void
net_addr_to_str(char* str, int len, const void* addr)
{
    const struct sockaddr* a = addr;
    if (a->sa_family == AF_INET)
        inet_ntop(a->sa_family, &((const struct sockaddr_in*)a)->sin_addr, str, len);
    else if (a->sa_family == AF_INET6)
        inet_ntop(a->sa_family, &((const struct sockaddr_in6*)a)->sin6_addr, str, len);
    else
        str[0] = '\0';
}

/* ------------------------------------------------------------------------- */
int
net_init(void)
{
    return 0;
}

/* ------------------------------------------------------------------------- */
void
net_deinit(void)
{
}

/* ------------------------------------------------------------------------- */
void
net_log_host_ips(void)
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

        /*s = getnameinfo(p->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);*/
    }

    freeifaddrs(ifaddr);
}

/* ------------------------------------------------------------------------- */
int
net_bind(
    const char* bind_address,
    const char* port,
    int* addrlen)
{
    struct addrinfo hints;
    struct addrinfo* candidates;
    struct addrinfo* p;
    char ipstr[INET6_ADDRSTRLEN];
    int ret;
    int sockfd;

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
        net_addr_to_str(ipstr, sizeof ipstr, p->ai_addr);

        log_dbg("Attempting to bind UDP %s:%s\n", ipstr, port);
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
            log_warn("bind() failed for UDP %s:%s: %s\n", ipstr, port, strerror(errno));
            close(sockfd);
        }

        *addrlen = p->ai_addrlen;
        break;
    }
    freeaddrinfo(candidates);
    if (p == NULL)
    {
        log_err("Failed to bind UDP socket\n");
        return -1;
    }

    log_dbg("Bound UDP socket to %s:%s\n", ipstr, port);
    return sockfd;
}

/* ------------------------------------------------------------------------- */
int
net_connect(struct cs_vector* sockfds, const char* server_address, const char* port)
{
    struct addrinfo hints;
    struct addrinfo* candidates;
    struct addrinfo* p;
    char ipstr[INET6_ADDRSTRLEN];
    int ret;
    int sockfd;

    assert(vector_count(sockfds) == 0);

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
        net_addr_to_str(ipstr, sizeof ipstr, p->ai_addr);

        log_dbg("Attempting to connect UDP socket %s:%s...\n", ipstr, port);
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
            log_warn("connect() failed for UDP %s:%s: %s\n", ipstr, port, strerror(errno));
            close(sockfd);
            continue;
        }

        log_dbg("Connected UDP socket to %s:%s\n", ipstr, port);
        vector_push(sockfds, &sockfd);
    }
    freeaddrinfo(candidates);

    if (vector_count(sockfds) == 0)
    {
        log_err("Failed to connect any UDP socket\n");
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
void
net_close(int sockfd)
{
    log_dbg("Closing socket\n");
    close(sockfd);
}

/* ------------------------------------------------------------------------- */
int
net_sendto(int sockfd, const char* buf, int len, const void* addr, int addrlen)
{
    return sendto(sockfd, buf, len, 0, addr, addrlen);
}

/* ------------------------------------------------------------------------- */
int
net_send(int sockfd, const char* buf, int len)
{
    return send(sockfd, buf, len, 0);
}

/* ------------------------------------------------------------------------- */
int
net_recvfrom(int sockfd, char* buf, int capacity, void* addr, int addrlen)
{
    struct sockaddr_storage addr_received;
    socklen_t addrlen_received = sizeof(addr_received);

    int bytes_received = recvfrom(
        sockfd, buf, capacity, 0,
        (struct sockaddr*)&addr_received, &addrlen_received);

    if (bytes_received < 0)
    {
        if (errno == EAGAIN)
            return 0;
        log_err("Receive call failed: %s\n", strerror(errno));
        return -1;
    }

    if ((int)addrlen_received != addrlen)
    {
        char ipstr[INET6_ADDRSTRLEN];
        net_addr_to_str(ipstr, INET6_ADDRSTRLEN, &addr_received);
        log_err("Received data from an address that has a different length than expected!\n");
        log_err("Expected: %d, received: %d, address: %s\n", addrlen, addrlen_received, ipstr);
        return 0;
    }

    memcpy(addr, &addr_received, addrlen);
    return bytes_received;
}

/* ------------------------------------------------------------------------- */
int
net_recv(int sockfd, char* buf, int capacity)
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
