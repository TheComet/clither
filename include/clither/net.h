#pragma once

#include "clither/config.h"
#include "cstructures/vector.h"

struct server
{
    int udp_sock;
    struct cs_vector pending_reliable;
};

struct client
{
    int udp_sock;
    struct cs_vector pending_reliable;
};

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

/*!
 * \brief Initialize a server structure and binds a socket to the specified
 * address.
 * \param[in] bind_address The address to bind() to. Set this to an empty
 * string to bind to the wildcard address.
 * \param[in] port The port to bind to.
 * \return Returns 0 if successful, -1 if unsuccessful.
 */
int
server_init(struct server* server, const char* bind_address, const char* port);

/*!
 * \brief Closes all sockets and frees all data.
 */
void
server_deinit(struct server* server);

/*!
 * \brief Fills all pending data into UDP packets and sends them to all clients.
 */
void
server_send_pending_data(struct server* server);

/*!
 * 
 */
void
server_recv(struct server* server);

/*!
 * \brief Initializes a client structure and resolves the host address.
 * \param[in] server_address Address of the server to connect to.
 * \param[in] port The port of the server to connect to.
 * \return Returns 0 if successful, -1 if unsuccessful.
 */
int
client_init(struct client* client, const char* server_address, const char* port);

void
client_deinit(struct client* client);

void
client_send_pending_data(struct client* client);
