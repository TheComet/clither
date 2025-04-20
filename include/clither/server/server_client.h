#pragma once

#define CBF_WINDOW_SIZE 20

#include <stdint.h> /* uint16_t */

struct food_in_proximity_hset;
struct net_server_interface;
struct net_server;
struct msg_vec;
struct snakes_in_proximity_bmap;

struct server_client
{
    const struct net_server_interface* inet;
    struct net_server*                 net;
    struct msg_vec*                    pending_msgs;
    struct snakes_in_proximity_bmap*   snakes_in_proximity;
    struct food_in_proximity_hset*     food_in_proximity;
    int                                timeout_counter;
    int      cbf_window[CBF_WINDOW_SIZE]; /* "Command Buffer Fullness" window */
    uint16_t snake_id;
    uint16_t last_command_msg_frame;
};

void server_client_init(
    struct server_client*              client,
    const struct net_server_interface* inet,
    struct net_server*                 net,
    uint16_t                           snake_id,
    uint16_t                           frame_number,
    uint8_t                            sim_tick_rate,
    uint8_t                            net_tick_rate);
void server_client_deinit(struct server_client* client);
