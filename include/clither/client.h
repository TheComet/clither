#pragma once

#include "clither/config.h"
#include "cstructures/vector.h"

C_BEGIN

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
    CLIENT_JOINED
};

struct client
{
    struct cs_vector pending_unreliable;  /* struct net_msg* */
    struct cs_vector pending_reliable;    /* struct net_msg* */
    int udp_sock;
    int sim_tick_rate;
    int net_tick_rate;
    int timeout_counter;
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
client_connect(struct client* client, const char* server_address, const char* port);

void
client_disconnect(struct client* client);

void
client_queue_unreliable(struct client* client, struct msg* m);

void
client_queue_reliable(struct client* client, struct msg* m);

int
client_send_pending_data(struct client* client);

int
client_recv_joining(struct client* client, uint16_t* frame_number);

int
client_recv_game(struct client* client);

C_END