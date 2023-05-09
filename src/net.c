#include "clither/log.h"
#include "clither/msg.h"
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
#   include <fcntl.h>
#endif

/*
 * 576 = minimum maximum reassembly buffer size
 * 60  = maximum IP header size
 * 8   = UDP header size
 */
#define MAX_UDP_PACKET_SIZE (576 - 60 - 8)

/* Unfortunate */
#if !defined(_WIN32)
#   define closesocket(s) close(s)
#endif

struct client_table_entry
{
    struct cs_vector pending_unreliable;  /* struct msg* */
    struct cs_vector pending_reliable;    /* struct msg* */
    int timeout_counter;
};

#define MSG_FOR_EACH(v, var) \
    VECTOR_FOR_EACH(v, struct msg*, vec_##var) \
    struct msg* var = *vec_##var; {
#define MSG_END_EACH \
    } VECTOR_END_EACH
#define MSG_ERASE_IN_FOR_LOOP(v, var) \
    VECTOR_ERASE_IN_FOR_LOOP(v, struct msg*, vec_##var)

#define CLIENT_TABLE_FOR_EACH(table, k, v) \
    HASHMAP_FOR_EACH(table, struct sockaddr, struct client_table_entry, k, v)
#define CLIENT_TABLE_END_EACH \
    HASHMAP_END_EACH

#define MALICIOUS_CLIENT_FOR_EACH(table, k, v) \
    HASHMAP_FOR_EACH(table, struct sockaddr, int, k, v)
#define MALICIOUS_CLIENT_END_EACH \
    HASHMAP_END_EACH

/* ------------------------------------------------------------------------- */
static void
msg_queue_init(struct cs_vector* q)
{
    vector_init(q, sizeof(struct msg*));
}
static void
msg_queue_deinit(struct cs_vector* q)
{
    MSG_FOR_EACH(q, m)
        msg_free(m);
    MSG_END_EACH
    vector_deinit(q);
}
static void
msg_queue_remove_type(struct cs_vector* q, enum msg_type type)
{
    MSG_FOR_EACH(q, msg)
        if (msg->type == type)
        {
            msg_free(msg);
            MSG_ERASE_IN_FOR_LOOP(q, msg);
        }
    MSG_END_EACH
}

/* ------------------------------------------------------------------------- */
static int
set_nonblocking(int sockfd)
{
#if defined(_WIN32)
    unsigned long nonblock = 1;
    if (ioctlsocket(sockfd, FIONBIO, &nonblock) == 0)
        return 0;
    log_err("ioctlsocket() failed for socket\n");
    return -1;
#else
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
#endif
}

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

    if (getifaddrs(&ifaddr) < 0)
    {
        log_warn("Failed to get host IP using getifaddrs()\n");
        return;
    }

    for (p = ifaddr; p; p = p->ifa_next)
    {
        if (p->ifa_addr == NULL)
            continue;

        //s = getnameinfo(p->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
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

        /* We want non-blocking sockets */
        if (set_nonblocking(server->udp_sock) < 0)
        {
            closesocket(server->udp_sock);
            continue;
        }

        if (bind(server->udp_sock, p->ai_addr, p->ai_addrlen) != 0)
        {
            log_warn("bind() failed for UDP %s:%s: %s\n", ipstr, port, strerror(errno));
            closesocket(server->udp_sock);
        }
        break;
    }
    if (p == NULL)
    {
        log_err("Failed to bind UDP socket\n");
        freeaddrinfo(candidates);
        return -1;
    }

    log_dbg("Bound UDP socket to %s:%s\n", ipstr, port);

    /*
     * Whenever we receive a UDP packet, we look up the source address to get
     * the client structure associated with that packet. Depending on whether
     * we are using IPv4 or IPv6 the size of the key will be different.
     */
    hashmap_init(
        &server->client_table,
        p->ai_addrlen,
        sizeof(struct client_table_entry));
    hashmap_init(
        &server->malicious_clients,
        p->ai_addrlen,
        sizeof(int));
    hashmap_init(
        &server->banned_clients,
        p->ai_addrlen,
        0);

    freeaddrinfo(candidates);

    return 0;
}

/* ------------------------------------------------------------------------- */
void
server_deinit(struct server* server)
{
    log_dbg("Closing socket\n");
    closesocket(server->udp_sock);

    hashmap_deinit(&server->banned_clients);
    hashmap_deinit(&server->malicious_clients);

    CLIENT_TABLE_FOR_EACH(&server->client_table, addr, client)
        msg_queue_deinit(&client->pending_reliable);
        msg_queue_deinit(&client->pending_unreliable);
    CLIENT_TABLE_END_EACH
    hashmap_deinit(&server->client_table);
}

/* ------------------------------------------------------------------------- */
void
server_send_pending_data(struct server* server)
{
    char buf[MAX_UDP_PACKET_SIZE];
    char ipstr[INET6_ADDRSTRLEN];

    CLIENT_TABLE_FOR_EACH(&server->client_table, addr, client)
        uint8_t type;
        int len = 0;

        /* Append unreliable messages first */
        MSG_FOR_EACH(&client->pending_unreliable, msg)
            if (len + msg->payload_len + 2 > MAX_UDP_PACKET_SIZE)
                continue;

            type = (uint8_t)msg->type;
            memcpy(buf + 0, &type, 1);
            memcpy(buf + 1, &msg->payload_len, 1);
            memcpy(buf + 2, msg->payload, msg->payload_len);

            len += msg->payload_len + 2;
            msg_free(msg);
            MSG_ERASE_IN_FOR_LOOP(&client->pending_unreliable, msg);
        MSG_END_EACH

        /* Append reliable messages */
        MSG_FOR_EACH(&client->pending_reliable, msg)
            if (len + msg->payload_len + 2 > MAX_UDP_PACKET_SIZE)
                continue;

            if (msg->resend_rate_counter-- > 0)
                continue;
            msg->resend_rate_counter = msg->resend_rate;

            type = (uint8_t)msg->type;
            memcpy(buf + 0, &type, 1);
            memcpy(buf + 1, &msg->payload_len, 1);
            memcpy(buf + 2, msg->payload, msg->payload_len);

            len += msg->payload_len + 2;
        MSG_END_EACH

        if (len > 0)
        {
            sockaddr_to_str(ipstr, addr, sizeof ipstr);
            log_dbg("Sending UDP packet to %s, size=%d\n", ipstr, len);
            /* NOTE: The hashmap's key size contains the length of the stored address */
            sendto(server->udp_sock, buf, len, 0, (struct sockaddr*)addr, server->client_table.key_size);
            client->timeout_counter++;
        }

        if (client->timeout_counter > 100)
        {
            sockaddr_to_str(ipstr, addr, sizeof ipstr);
            log_warn("Client %s timed out\n", ipstr);
            msg_queue_deinit(&client->pending_reliable);
            msg_queue_deinit(&client->pending_unreliable);
            hashmap_erase(&server->client_table, addr);
        }
    CLIENT_TABLE_END_EACH
}

/* ------------------------------------------------------------------------- */
static void
server_client_add(struct server* server)
{

}

/* ------------------------------------------------------------------------- */
static void
mark_client_as_malicious_and_drop(struct server* server)
{

}

/* ------------------------------------------------------------------------- */
int
server_recv(struct server* server)
{
    char buf[MAX_UDP_PACKET_SIZE];
    char ipstr[INET6_ADDRSTRLEN];
    struct client_table_entry* client;
    struct sockaddr_in6 client_addr;  /* Assumption is: IPv6 is the largest address possible */
    socklen_t client_addr_len;
    int bytes_received;
    int i;

    /* Update timeout counters of every client that we've communicated with */
    CLIENT_TABLE_FOR_EACH(&server->client_table, addr, client)
        client->timeout_counter++;
    CLIENT_TABLE_END_EACH

    /* Update malicious client timeouts */
    MALICIOUS_CLIENT_FOR_EACH(&server->malicious_clients, addr, timeout)
        if (--(*timeout) <= 0)
        {
            sockaddr_to_str(ipstr, addr, sizeof ipstr);
            log_dbg("Client %s removed from malicious list\n", ipstr);
            hashmap_erase(&server->malicious_clients, addr);
        }
    MALICIOUS_CLIENT_END_EACH

    /* We may need to read more than one UDP packet */
    while (1)
    {
        memset(&client_addr, 0, sizeof client_addr);
        client_addr_len = sizeof(client_addr);
        bytes_received = recvfrom(server->udp_sock, buf, MAX_UDP_PACKET_SIZE, 0, (struct sockaddr*)&client_addr, &client_addr_len);

        /* Call to recvfrom is non-blocking, handle accordingly */
        if (bytes_received < 0)
        {
#if defined(_WIN32)
            if (WSAGetLastError() == WSAEWOULDBLOCK)
                return 0;
            log_err("Receive call failed: %d\n", WSAGetLastError());
            return -1;
#else
            if (errno == EAGAIN)
                return 0;
            log_err("Receive call failed: %s\n", strerror(errno));
            return -1;
#endif
        }

        /*
         * If we received a packet from a banned client, ignore packet
         */
        if (hashmap_find(&server->banned_clients, &client_addr) != 0)
            continue;

        /*
         * If we received a packet from a potentially malicious client, increase their timeout
         */
        {
            int* timeout = hashmap_find(&server->malicious_clients, &client_addr);
            if (timeout != NULL)
            {
                *timeout += 20 * 60;  /* 1 minute */
                continue;
            }
        }

        /*
         * If we received a packet from a registered client, reset their timeout
         * counter
         */
        client = hashmap_find(&server->client_table, &client_addr);
        if (client != NULL)
            client->timeout_counter = 0;

        /* 
         * Packet can contain multiple message objects.
         * buf[0] == message type
         * buf[1] == payload length
         * buf[2] == beginning of message payload
         */
        for (i = 0; i < bytes_received - 1;)
        {
            union parsed_payload pp;
            enum msg_type type = buf[i+0];
            uint8_t payload_len = buf[i+1];
            if (i+2 + payload_len > bytes_received)
            {
                sockaddr_to_str(ipstr, (struct sockaddr*)&client_addr, sizeof ipstr);
                log_warn("Invalid payload length \"%d\" received from client %s\n", (int)payload_len, ipstr);
                log_warn("Dropping rest of packet\n");
                mark_client_as_malicious_and_drop(server);
                break;
            }

            /*
             * Disallow receiving packets from clients that are not registered
             * with the excepption of the "join game request" message.
             */
            if (client == NULL && type != MSG_JOIN_REQUEST)
            {
                sockaddr_to_str(ipstr, (struct sockaddr*)&client_addr, sizeof ipstr);
                log_warn("Received packet from unknown client %s, ignoring\n", ipstr);
                break;
            }

            /* 
             * NOTE: Beyond this point, "client" won't be NULL *unless* the message is
             * MSG_JOIN_REQUEST. This makes the switch/case handling a little easier for
             * all other cases
             */

            /* Process message */
            switch (msg_parse_paylaod(&pp, type, payload_len, &buf[i+2]))
            {
                default: {
                    mark_client_as_malicious_and_drop(server);
                } break;


                case MSG_JOIN_REQUEST: {
                    struct msg* response;
                    struct qpos2 spawn_pos = { 32, 32 };

                    if (client != NULL)
                    {

                    }

                    if (hashmap_count(&server->client_table) > 600)
                    {
                        char response[2] = { MSG_JOIN_DENY_SERVER_FULL, 0 };
                        sendto(server->udp_sock, response, 2, 0, (struct sockaddr*)&client_addr, client_addr_len);
                        break;
                    }

                    if (pp.join_request.username_len > 32)
                    {
                        char response[2] = { MSG_JOIN_DENY_BAD_USERNAME, 0 };
                        sendto(server->udp_sock, response, 2, 0, (struct sockaddr*)&client_addr, client_addr_len);
                        break;
                    }

                    if (client == NULL)
                    {
                        /* TODO: Create snake in world */

                        log_dbg("Protocol: MSG_JOIN <- \"%s\"\n", pp.join_request.username);

                        client = hashmap_emplace(&server->client_table, &client_addr);
                        msg_queue_init(&client->pending_reliable);
                        msg_queue_init(&client->pending_unreliable);
                    }

                    response = msg_join_accept(&spawn_pos);
                    vector_push(&client->pending_unreliable, &response);
                } break;

                case MSG_JOIN_ACCEPT:
                case MSG_JOIN_DENY_BAD_PROTOCOL:
                case MSG_JOIN_DENY_BAD_USERNAME:
                case MSG_JOIN_DENY_SERVER_FULL:
                    break;

                case MSG_JOIN_ACK: {
                } break;

                case MSG_LEAVE: {

                } break;

                case MSG_SNAKE_METADATA: {

                } break;

                case MSG_SNAKE_METADATA_ACK: {

                } break;

                case MSG_SNAKE_DATA: {

                } break;

                case MSG_FOOD_CREATE: {

                } break;

                case MSG_FOOD_CREATE_ACK: {

                } break;

                case MSG_FOOD_DESTROY: {

                } break;

                case MSG_FOOD_DESTROY_ACK: {

                } break;
            }

            i += payload_len + 2;
        }
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

        /* We want non-blocking sockets */
        if (set_nonblocking(client->udp_sock) < 0)
        {
            closesocket(client->udp_sock);
            continue;
        }

        /*
         * When connecting a UDP socket, we can use send() instead of sendto()
         * This way, the server address (socketaddr_storage) doens't need to be
         * saved in the client structure
         */
        if (connect(client->udp_sock, p->ai_addr, p->ai_addrlen) != 0)
        {
            log_warn("connect() failed for UDP %s:%s: %s\n", ipstr, port, strerror(errno));
            closesocket(client->udp_sock);
        }
        break;
    }
    freeaddrinfo(candidates);
    if (p == NULL)
    {
        log_err("Failed to connect UDP socket\n");
        return -1;
    }

    log_dbg("Connected UDP socket to %s:%s\n", ipstr, port);

    msg_queue_init(&client->pending_unreliable);
    msg_queue_init(&client->pending_reliable);
    client->timeout_counter = 0;

    return 0;
}

/* ------------------------------------------------------------------------- */
void
client_deinit(struct client* client)
{
    log_dbg("Closing socket\n");
    closesocket(client->udp_sock);

    msg_queue_deinit(&client->pending_reliable);
    msg_queue_deinit(&client->pending_unreliable);
}

/* ------------------------------------------------------------------------- */
int
client_send_pending_data(struct client* client)
{
    uint8_t type;
    char buf[MAX_UDP_PACKET_SIZE];
    int len = 0;

    /* Append unreliable messages first */
    MSG_FOR_EACH(&client->pending_unreliable, msg)
        if (len + msg->payload_len + 2 > MAX_UDP_PACKET_SIZE)
            continue;

        type = (uint8_t)msg->type;
        memcpy(buf + 0, &type, 1);
        memcpy(buf + 1, &msg->payload_len, 1);
        memcpy(buf + 2, msg->payload, msg->payload_len);

        len += msg->payload_len + 2;
        msg_free(msg);
        MSG_ERASE_IN_FOR_LOOP(&client->pending_unreliable, msg);
    MSG_END_EACH

    /* Append reliable messages */
    MSG_FOR_EACH(&client->pending_reliable, msg)
        if (len + msg->payload_len + 2 > MAX_UDP_PACKET_SIZE)
            continue;

        if (msg->resend_rate_counter-- > 0)
            continue;
        msg->resend_rate_counter = msg->resend_rate;

        type = (uint8_t)msg->type;
        memcpy(buf + 0, &type, 1);
        memcpy(buf + 1, &msg->payload_len, 1);
        memcpy(buf + 2, msg->payload, msg->payload_len);

        len += msg->payload_len + 2;
    MSG_END_EACH

    if (len > 0)
    {
        log_dbg("Sending UDP packet, size=%d\n", len);
        send(client->udp_sock, buf, len, 0);
        client->timeout_counter++;
    }

    if (client->timeout_counter > 100)
    {
        log_err("Server timed out\n");
        return -1;
    }

    return 0;
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
    bytes_received = recvfrom(client->udp_sock, buf, MAX_UDP_PACKET_SIZE, 0, (struct sockaddr*)&server_addr, &server_addr_len);

    /* Call to recvfrom is non-blocking, handle accordingly */
    if (bytes_received < 0)
    {
#if defined(_WIN32)
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return 0;
        if (WSAGetLastError() == WSAECONNRESET)
            return 0;
        log_err("Receive call failed: %d\n", WSAGetLastError());
        return -1;
#else
        if (errno == EAGAIN)
            return 0;
        log_err("Receive call failed: %s\n", strerror(errno));
        return -1;
#endif
    }

    /* Packet can contain multiple message objects. Unpack */
    for (i = 0; i < bytes_received - 1;)
    {
        union parsed_payload pp;
        enum net_msg_type type = buf[i+0];
        uint8_t payload_len = buf[i+1];
        if (i+2 + payload_len > bytes_received)
        {
            sockaddr_to_str(ipstr, (struct sockaddr*)&server_addr, sizeof ipstr);
            log_warn("Invalid payload length \"%d\" received from %s\n", (int)payload_len, ipstr);
            log_warn("Dropping rest of packet\n");
            return 0;
        }

        /* Process message */
        switch (msg_parse_paylaod(&pp, type, payload_len, &buf[i+2]))
        {
            default: {
                log_warn("Received unknown message type from server. Malicious?\n");
            } break;

            case MSG_JOIN_REQUEST:
                break;

            case MSG_JOIN_ACCEPT: {
                /* Stop making join game requests */
                msg_queue_remove_type(&client->pending_reliable, MSG_JOIN_REQUEST);

                log_dbg("MSG_JOIN_ACCEPT: %d, %d\n", pp.join_accept.spawn.x, pp.join_accept.spawn.y);
            } break;

            case MSG_JOIN_DENY_BAD_PROTOCOL:
            case MSG_JOIN_DENY_BAD_USERNAME:
            case MSG_JOIN_DENY_SERVER_FULL: {
                log_err("Failed to join server: %s\n", pp.join_deny.error);
            } break;

            case MSG_JOIN_ACK:
                break;
        }

        i += payload_len + 2;
    }

    client->timeout_counter = 0;
    return 0;
}
