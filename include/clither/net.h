#pragma once

#include "clither/config.h"
#include "clither/server_settings.h"
#include "cstructures/vector.h"
#include "cstructures/hashmap.h"

C_BEGIN

struct server
{
    struct server_settings settings;
    struct cs_hashmap client_table;       /* struct (key size depends on protocol) -> struct client_table_entry (see net.c) */
    struct cs_hashmap malicious_clients;  /* struct sockaddr (key size depends on protocol) */
    struct cs_hashmap banned_clients;     /* struct sockaddr (key size depends on protocol) */
    int udp_sock;
};

struct client
{
    struct cs_vector pending_unreliable;  /* struct net_msg* */
    struct cs_vector pending_reliable;    /* struct net_msg* */
    int udp_sock;
    int timeout_counter;
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
server_init(
    struct server* server,
    const char* bind_address,
    const char* port,
    const char* config_filename);

/*!
 * \brief Closes all sockets and frees all data.
 */
void
server_deinit(struct server* server, const char* config_filename);

/*!
 * \brief Fills all pending data into UDP packets and sends them to all clients.
 */
void
server_send_pending_data(struct server* server);

/*!
 *
 */
int
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

int
client_send_pending_data(struct client* client);

int
client_recv(struct client* client);

C_END
