#pragma once

#include "clither/config.h"

struct net_addr;
struct net_addr_hmap;
struct server_client_hmap;
struct settings;
struct settings_server;
struct settings_world;
struct server_client;
struct world;

struct server
{
    struct server_client_hmap* clients;
    struct net_addr_hmap*      malicious_clients;
    struct net_addr_hmap*      banned_clients;

    int udp_sock;
#if defined(CLITHER_SERVER_WEBSOCKETS)
    int tcp_sock;
#endif
};

/*!
 * \brief Initialize a server structure and binds a socket to the specified
 * address.
 * \param[in] bind_address The address to bind() to. Set this to an empty
 * string to bind to the wildcard address.
 * \param[in] port The port to bind to. If you specify an empty string then the
 * value from the config file will be used (and if that doesn't exist then the
 * default port will be used).
 * \return Returns 0 if successful, -1 if unsuccessful.
 */
int server_init(
    struct server* server, const char* bind_address, const char* port);

/*!
 * \brief Closes all sockets and frees all data.
 * \param[in] server The server to free.
 */
void server_deinit(struct server* server);

int server_update_snakes_in_range(
    struct server* server, const struct world* world);

int server_kill_snake_checks(struct server* server, struct world* world);

int server_queue_snake_data(
    struct server* server, const struct world* world, uint16_t frame_number);

int server_queue_food_data(struct server* server, const struct world* world);

/*!
 * \brief Fills all pending data into UDP packets and sends them to all clients.
 */
int server_send_pending_data(struct server* server, struct world* world);

/*!
 *
 */
int server_recv(
    struct server*                server,
    const struct settings_server* settings_server,
    struct world*                 world,
    const struct settings_world*  settings_world,
    uint16_t                      frame_number);

void* server_run(const void* args);
