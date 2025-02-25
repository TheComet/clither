#pragma once

#include "clither/config.h"
#include "clither/hm.h"

struct server_settings;
struct world;

HM_DECLARE(client_hm, struct net_addr, struct server_client*, 16)
HM_DECLARE(malicious_clients_hm, struct net_addr, int, 16)
HM_DECLARE(banned_clients_hm, struct net_addr, void, 16)

struct server
{
    /* struct (key size depends on protocol) -> struct client_table_entry */
    /* (see net.c) */
    struct client_hm* client_table;
    /* struct sockaddr (key size depends on protocol) */
    struct malicious_clients_hm* malicious_clients;
    /* struct sockaddr (key size depends on protocol) */
    struct banned_clients_hm* banned_clients;

    int udp_sock;
};

/*!
 * \brief Initialize a server structure and binds a socket to the specified
 * address.
 * \param[in] bind_address The address to bind() to. Set this to an empty
 * string to bind to the wildcard address.
 * \param[in] port The port to bind to. If you specify an empty string then the
 * value from the config file will be used (and if that doesn't exist then the
 * default port will be used).
 * \param[in] config_filename The name of the config.ini file to load settings
 * from. Note that if you pass in
 * \return Returns 0 if successful, -1 if unsuccessful.
 */
int server_init(
    struct server* server, const char* bind_address, const char* port);

/*!
 * \brief Closes all sockets and frees all data. Saves the settings structure
 * back to the config.ini file.
 * \param[in] server The server to free.
 * \param[in] config_filename The name of the config.ini file to save settings
 * to.
 */
void server_deinit(struct server* server);

void server_queue_snake_data(
    struct server* server, const struct world* world, uint16_t frame_number);

/*!
 * \brief Fills all pending data into UDP packets and sends them to all clients.
 */
void server_send_pending_data(struct server* server);

/*!
 *
 */
int server_recv(
    struct server*                server,
    const struct server_settings* settings,
    struct world*                 world,
    uint16_t                      frame_number);

void* server_run(const void* args);

C_END
