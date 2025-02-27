#pragma once

#include "clither/vec.h"

/*
 * 576 = minimum maximum reassembly buffer size
 * 60  = maximum IP header size
 * 8   = UDP header size
 */
#define NET_MAX_UDP_PACKET_SIZE (576 - 60 - 8)

/*
 * In this implementation, IPv6 is the largest address used. The
 * size of struct sockaddr_in6 on windows and linux are 28 bytes.
 */
#define NET_MAX_ADDRLEN    32
#define NET_MAX_ADDRSTRLEN 65

#define NET_DEFAULT_PORT    "5555"
#define NET_DEFAULT_ADDRESS "ws://127.0.0.1:8080"

VEC_DECLARE(sockfd_vec, int, 8)

struct net_addr
{
    int  len;
    char sockaddr_storage[NET_MAX_ADDRLEN];
};

struct net_udp_packet
{
    int     len;
    uint8_t data[NET_MAX_UDP_PACKET_SIZE];
};

struct net_addr_str
{
    char cstr[NET_MAX_ADDRSTRLEN];
};

/*!
 * \brief Initialize global data for networking. Call this before any other
 * net function.
 * \return Returns 0 on success, -1 on failure.
 */
int net_init(void);

/*!
 * \brief Clean up global data for networking. Call at the very end.
 */
void net_deinit(void);

/*!
 * \brief Prints out the host name, host IP, and makes a request to an IP echo
 * server to get the public-facing IP address.
 */
void net_log_host_ips(void);

/*
 * \brief Converts an address into a readable string (either IPv4 or IPv6
 * address).
 * \note The output string is guaranteed to be null-terminated.
 */
void net_addr_to_str(struct net_addr_str* str, const struct net_addr* addr);

/*!
 * \brief Creates a non-blocking socket and binds it to the specified address.
 * This function is designed to work with server_init() and net_sendto().
 * \param[in] bind_address Address to bind to. If you pass in an empty string
 * then the address will be determined automatically.
 * \param[in] port Port to bind to.
 * \param[out] addrlen The length of the bind address. Because other parts of
 * the program store network addresses (for example the ban list), the length
 * of the incoming addresses (IPv4 or IPv6) needs to be known.
 * \return Returns the socket file descriptor, or -1 if an error occurred.
 */
int net_bind(const char* bind_address, const char* port);

/*!
 * \brief Creates a number of non-blocking sockets and connects them to the
 * specified address.
 * \note Because these are UDP sockets, we can't know ahead of time whether the
 * server is using IPv6 or IPv4. This is handled later when sending the first
 * packets by using the first socket that responds successfully and closing the
 * rest.
 * \note This function is designed to work with client_init() and net_send().
 * All sockets are "connected" to the server address, meaning, you can use
 * net_send() instead of net_sendto(). This means the server address doesn't
 * have to be stored in the client structure.
 * \param[out] sockfds Pointer to an initialized vector. All potential socket
 * file descriptors are pushed into this vector.
 * \param[in] server_address The server address to connect to.
 * \param[in] port The port of the server to connect to.
 * \return Returns 0 on success and -1 if an error occurred.
 */
int net_connect(
    struct sockfd_vec** sockfds, const char* server_address, const char* port);

/*! Closes a socket */
void net_close(int sockfd);

/*!
 * \brief Sends data to the specified address (UDP).
 */
int net_sendto(
    int sockfd, const void* buf, int len, const struct net_addr* addr);

/*!
 * \brief Sends data over a previously connected socket (see
 * net_connect_socket()).
 */
int net_send(int sockfd, const void* buf, int len);

/*!
 * \brief Receive data (non-blocking) from a socket. Data is written to buf
 * and the number of bytes received is returned. The address length returned
 * from recvfrom() must match addrlen exactly or this function will fail.
 *
 * \return Returns 0 if nothing was received. Returns -1 if an error occurred.
 * Returns the number of bytes received if successful.
 */
int net_recvfrom(int sockfd, void* buf, int capacity, struct net_addr* addr);

/*!
 * \brief Receive data (non-blocking) from a connected socket. Data is written
 * to buf and the number of bytes received is returned.
 *
 * \return Returns 0 if nothing was received. Returns -1 if an error occurred.
 * Returns the number of bytes received if successful.
 */
int net_recv(int sockfd, void* buf, int capacity);
