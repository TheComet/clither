#pragma once

#include "clither/config.h"
#if defined(CLITHER_GFX)

#    include <stdint.h>

struct msg;
struct msg_vec;
struct world;

/*
 * The client can be in 3 states:
 *   - Disconnected, or "menu mode"
 *   - Attempting to connect
 *   - Connected and simulating
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

static inline struct client_recv_result client_recv_ok(void)
{
    struct client_recv_result r = {0, 0, 0};
    return r;
}
static inline struct client_recv_result client_recv_error(void)
{
    struct client_recv_result r = {0, 0, 0};
    r.error = 1;
    return r;
}
static inline struct client_recv_result client_recv_disconnected(void)
{
    struct client_recv_result r = {0, 0, 0};
    r.disconnected = 1;
    return r;
}
static inline struct client_recv_result client_recv_tick_rate_changed(void)
{
    struct client_recv_result r = {0, 0, 0};
    r.tick_rated_changed = 1;
    return r;
}
static inline struct client_recv_result client_recv_result_combine(
    struct client_recv_result r1, struct client_recv_result r2)
{
    r1.error |= r2.error;
    r1.disconnected |= r2.disconnected;
    r1.tick_rated_changed |= r2.tick_rated_changed;
    return r1;
}

struct client
{
    struct str*        username;
    struct msg_vec*    pending_msgs;
    struct sockfd_vec* udp_sockfds;
    int                timeout_counter;
    uint16_t           frame_number; /* Counts upwards at sim_tick_rate */
    uint16_t           snake_id;
    int16_t            warp;
    uint8_t            sim_tick_rate;
    uint8_t            net_tick_rate;
    enum client_state  state;
};

/*!
 * \brief Initializes a client structure. The client will be unconnected
 * by default. Use client_connect() to connect to a server.
 */
void client_init(struct client* client);

void client_deinit(struct client* client);

/*!
 * \brief Initializes a client structure and resolves the host address.
 * \param[in] server_address Address of the server to connect to.
 * \param[in] port The port of the server to connect to.
 * \return Returns 0 if successful, -1 if unsuccessful.
 */
int client_connect(
    struct client* client,
    const char*    server_address,
    const char*    port,
    const char*    username);

void client_disconnect(struct client* client);

int client_queue(struct client* client, struct msg* m);

int client_send_pending_data(struct client* client);

/*!
 * \brief
 * \return Returns -1 if an error occurs. Returns 1 if the client's state
 * changed. Returns 0 otherwise.
 */
struct client_recv_result
client_recv(struct client* client, struct world* world);

/*!
 * \brief The main loop of the client.
 * \warning This should function assumes that cs_init_threadlocal() was called.
 * If you want to run this function in a thread, then you have to manage all
 * threadlocal global state at the call-sight. This function was designed this
 * way because in all cases, after everything is initialized, the client will
 * run in the foreground.
 * \param[in] a Command line arguments.
 */
struct args;
void* client_run(const struct args* a);

#endif /* CLITHER_GFX */
