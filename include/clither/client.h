#pragma once

#include "clither/config.h"
#include "cstructures/vector.h"

C_BEGIN

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
    int sim_tick_rate;
    int net_tick_rate;
    int udp_sock;
    int timeout_counter;
    uint32_t frame_number;
    enum client_state state;
};

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
