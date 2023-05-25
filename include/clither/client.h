#pragma once

#include "clither/config.h"
#include "clither/controls.h"
#include "cstructures/btree.h"
#include "cstructures/vector.h"

C_BEGIN

struct msg;

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

struct client
{
    struct cs_vector pending_unreliable;  /* struct net_msg* */
    struct cs_vector pending_reliable;    /* struct net_msg* */
    struct cs_vector udp_sockfds;         /* int */
    struct cs_vector controls_buffer;     /* struct controls */
    int sim_tick_rate;
    int net_tick_rate;
    int timeout_counter;
    uint16_t frame_number;                /* Counts upwards at sim_tick_rate */
    uint16_t first_unackknowledged_controls_frame;  /* Will equal the last acknowledged controls frame + 1 */
    enum client_state state;
};

/*!
 * \brief Initializes a client structure. The client will be unconnected
 * by default. Use client_connect() to connect to a server.
 */
void
client_init(struct client* client);

void
client_deinit(struct client* client);

/*!
 * \brief Initializes a client structure and resolves the host address.
 * \param[in] server_address Address of the server to connect to.
 * \param[in] port The port of the server to connect to.
 * \return Returns 0 if successful, -1 if unsuccessful.
 */
int
client_connect(
    struct client* client,
    const char* server_address,
    const char* port,
    const char* username);

void
client_disconnect(struct client* client);

void
client_add_controls(struct client* client, const struct controls* controls);

void
client_ack_controls(struct client* client, uint16_t frame_number);

void
client_queue_controls(struct client* client);

static inline void
client_queue_unreliable(struct client* client, struct msg* m)
    { vector_push(&client->pending_unreliable, &m); }

static inline void
client_queue_reliable(struct client* client, struct msg* m)
    { vector_push(&client->pending_reliable, &m); }

int
client_send_pending_data(struct client* client);

/*!
 * \brief
 * \return Returns -1 if an error occurs. Returns 1 if the client's state
 * changed. Returns 0 otherwise.
 */
int
client_recv(struct client* client);

C_END
