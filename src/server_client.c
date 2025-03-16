#include "clither/bmap.h"
#include "clither/msg.h"
#include "clither/msg_vec.h"
#include "clither/proximity_state.h"
#include "clither/proximity_state_bmap.h"
#include "clither/server_client.h"
#include "clither/vec.h"

/* ------------------------------------------------------------------------- */
void server_client_init(
    struct server_client* client,
    uint16_t              snake_id,
    uint16_t              frame_number,
    uint8_t               sim_tick_rate,
    uint8_t               net_tick_rate)
{
    int cbf_idx;

    msg_vec_init(&client->pending_msgs);
    proximity_state_bmap_init(&client->snakes_in_proximity);
    client->timeout_counter = 0;
    client->snake_id = snake_id;
    client->last_command_msg_frame = frame_number;

    /*
     * Init "Command Buffer Fullness" queue with minimum
     * granularity. This assumes the client has the most
     * stable connection initially.
     */
    for (cbf_idx = 0; cbf_idx != CBF_WINDOW_SIZE; ++cbf_idx)
        client->cbf_window[cbf_idx] = sim_tick_rate / net_tick_rate;
}

/* ------------------------------------------------------------------------- */
void server_client_deinit(struct server_client* client)
{
    struct msg**            pmsg;
    struct proximity_state* prox;
    int16_t                 idx;
    uint16_t                snake_id;

    vec_for_each (client->pending_msgs, pmsg)
        msg_free(*pmsg);
    msg_vec_deinit(client->pending_msgs);

    bmap_for_each (client->snakes_in_proximity, idx, snake_id, prox)
        (void)idx, (void)snake_id, proximity_state_deinit(prox);
    proximity_state_bmap_deinit(client->snakes_in_proximity);
}
