#pragma once

#include "clither/config.h"

/*
 * 576 = minimum maximum reassembly buffer size
 * 60  = maximum IP header size
 * 8   = UDP header size
 */
#define MAX_UDP_PACKET_SIZE (576 - 60 - 8)

/*
 * In this implementation, IPv6 is the largest address used. The
 * size of struct sockaddr_in6 on windows is 28 bytes.
 */
#define MAX_ADDRLEN 32

#define MAX_ADDRSTRLEN 65

#define NET_DEFAULT_PORT "5555"

C_BEGIN

/*!
 * \brief Initialize global data for networking. Call this before any other
 * net function.
 * \return Returns 0 on success, -1 on failure.
 */
int
net_init(void);

/*!
 * \brief Clean up global data for networking. Call at the very end.
 */
void
net_deinit(void);

/*!
 * \brief Prints out the host name, host IP, and makes a request to an IP echo
 * server to get the public-facing IP address.
 */
void
net_log_host_ips(void);

/*
 * \brief Converts an address into a readable string (either IPv4 or IPv6 address).
 * \note The output string is guaranteed to be null-terminated.
 */
void
net_addr_to_str(char* str, int len, const void* addr);

/*!
 * \brief Creates a non-blocking socket and binds it to the specified address.
 * This function is designed to work with server_init() and net_socket_sendto().
 * \param[in] bind_address Address to bind to. If you pass in an empty string
 * then the address will be determined automatically.
 * \param[in] port Port to bind to.
 * \param[out] addrlen The length of the bind address. Because other parts of
 * the program store network addresses (for example the ban list), the length
 * of the incoming addresses (IPv4 or IPv6) needs to be known.
 * \return Returns the socket file descriptor, or -1 if an error occurred.
 */
int
net_bind(const char* bind_address, const char* port, int* addrlen);

/*!
 * \brief Creates a non-blocking socket and connects it with the specified
 * address.
 * This function is designed to work with client_init() and net_socket_send().
 * \param[in] server_address The server address to connect to.
 * \param[in] port The port of the server to connect to.
 * \return Returns the socket file descriptor, or -1 if an error occurred.
 */
int
net_connect(const char* server_address, const char* port);

/*! Closes a socket */
void
net_close(int sockfd);

/*!
 * \brief Sends data to the specified address (UDP).
 */
int
net_sendto(int sockfd, const char* buf, int len, const void* addr, int addrlen);

/*!
 * \brief Sends data over a previously connected socket (see net_connect_socket()).
 */
int
net_send(int sockfd, const char* buf, int len);

/*!
 * \brief Receive data (non-blocking) from a socket. Data is written to buf
 * and the number of bytes received is returned. The address length returned
 * from recvfrom() must match addrlen exactly or this function will fail.
 * 
 * \return Returns 0 if nothing was received. Returns -1 if an error occurred.
 * Returns the number of bytes received if successful.
 */
int
net_recvfrom(int sockfd, char* buf, int capacity, void* addr, int addrlen);

/*!
 * \brief Receive data (non-blocking) from a connected socket. Data is written
 * to buf and the number of bytes received is returned.
 * 
 * \return Returns 0 if nothing was received. Returns -1 if an error occurred.
 * Returns the number of bytes received if successful.
 */
int
net_recv(int sockfd, char* buf, int capacity);

C_END
