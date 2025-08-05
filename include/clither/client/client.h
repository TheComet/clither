#pragma once

#include "clither/config.h"

#if defined(CLITHER_CLIENT)
#    include <stdint.h>

struct bot;
struct bot_interface;
struct fs_watch;
struct gfx;
struct gfx_interface;
struct msg;
struct msg_vec;
struct resource_pack;
struct settings;
struct settings_gfx;
struct settings_world;
struct world;

/*
 * The client can be in 3 states: - Disconnected, or "menu mode" - Attempting
 * to connect - Connected and simulating
 */
enum client_state
{
    CLIENT_DISCONNECTED,
    CLIENT_JOINING,
    CLIENT_CONNECTED
};

struct client_recv_result
{
    unsigned error : 1;        /* Critical error (e.g. oom) */
    unsigned disconnected : 1; /* The "client_state" property was changed */
    unsigned tick_rated_changed : 1; /* The server has adjusted the client's
                                        tick rate */
};

static struct client_recv_result client_recv_ok(void)
{
    struct client_recv_result r = {0, 0, 0};
    return r;
}
static struct client_recv_result client_recv_error(void)
{
    struct client_recv_result r = {0, 0, 0};
    r.error = 1;
    return r;
}
static struct client_recv_result client_recv_disconnected(void)
{
    struct client_recv_result r = {0, 0, 0};
    r.disconnected = 1;
    return r;
}
static struct client_recv_result client_recv_tick_rate_changed(void)
{
    struct client_recv_result r = {0, 0, 0};
    r.tick_rated_changed = 1;
    return r;
}
static struct client_recv_result client_recv_result_combine(
    struct client_recv_result r1, struct client_recv_result r2)
{
    r1.error |= r2.error;
    r1.disconnected |= r2.disconnected;
    r1.tick_rated_changed |= r2.tick_rated_changed;
    return r1;
}

struct client
{
    const struct net_client_interface* inet;
    struct net_connection*             connection;
    struct msg_vec*                    pending_msgs;

    struct str* username;

    int      timeout_counter;
    uint16_t frame_number; /* Increments at a frequency of sim_tick_rate */
    uint16_t snake_id;
    int16_t  warp;
    uint8_t  sim_tick_rate;
    uint8_t  net_tick_rate;
    enum client_state state;
};

/*! \brief Initializes a client structure. The client will be unconnected by
 * default. Use client_connect() to connect to a server.
 */
void client_init(struct client* client);

void client_deinit(struct client* client);

/*! \brief Initializes a client structure and resolves the host address.
 * \param[in] server_address Address of the server to connect to. \param[in]
 * port The port of the server to connect to. \return Returns 0 if successful,
 * -1 if unsuccessful.
 */
int client_connect(
    struct client* client,
    const char*    server_address,
    const char*    port,
    const char*    username);

void client_disconnect(struct client* client);

int client_queue(struct client* client, struct msg* m);

int client_send_pending_data(struct client* client);

/*! \brief \return Returns -1 if an error occurs. Returns 1 if the client's
 * state changed. Returns 0 otherwise.
 */
struct client_recv_result
client_recv(struct client* client, struct world* world);

/*! \brief The main loop of the client. Designed to be called from the main
 * thread. */
int client_run(
    struct client*               client,
    const struct settings*       settings,
    const struct gfx_interface** igfx,
    struct gfx**                 gfx,
    struct resource_pack**       pack,
    struct fs_watch**            pack_watch,
    const struct bot_interface*  ibot,
    struct bot*                  bot);

#endif
