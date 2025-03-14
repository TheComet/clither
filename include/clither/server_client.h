#pragma once

#define CBF_WINDOW_SIZE 20

#include <stdint.h> /* uint16_t */

struct msg_vec;
struct proximity_state_bmap;

struct server_client
{
    struct msg_vec*              pending_msgs;
    struct proximity_state_bmap* snakes_in_proximity;
    int                          timeout_counter;
    int      cbf_window[CBF_WINDOW_SIZE]; /* "Command Buffer Fullness" window */
    uint16_t snake_id;
    uint16_t last_command_msg_frame;
};
