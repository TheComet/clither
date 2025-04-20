#include "clither/game/msg.h"
#include "clither/game/msg_vec.h"
#include "clither/server/bezier_knot_acks_bmap.h"
#include "clither/server/food_in_proximity_hset.h"
#include "clither/server/server_client.h"
#include "clither/server/snakes_in_proximity_bmap.h"
#include "clither/util/bmap.h"
#include "clither/util/vec.h"

/* ------------------------------------------------------------------------- */
void server_client_init(
    struct server_client*              client,
    const struct net_server_interface* inet,
    struct net_server*                 net,
    uint16_t                           snake_id,
    uint16_t                           frame_number,
    uint8_t                            sim_tick_rate,
    uint8_t                            net_tick_rate)
{
    int cbf_idx;

    msg_vec_init(&client->pending_msgs);
    snakes_in_proximity_bmap_init(&client->snakes_in_proximity);
    food_in_proximity_hset_init(&client->food_in_proximity);
    client->inet = inet;
    client->net = net;
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
    struct msg**                   pmsg;
    struct bezier_knot_acks_bmap** knot_acks;
    int16_t                        idx;
    uint16_t                       snake_id;

    vec_for_each (client->pending_msgs, pmsg)
        msg_free(*pmsg);
    msg_vec_deinit(client->pending_msgs);

    food_in_proximity_hset_deinit(client->food_in_proximity);

    bmap_for_each (client->snakes_in_proximity, idx, snake_id, knot_acks)
        (void)idx, (void)snake_id, bezier_knot_acks_bmap_deinit(*knot_acks);
    snakes_in_proximity_bmap_deinit(client->snakes_in_proximity);
}
