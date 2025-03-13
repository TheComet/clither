#pragma once

#define CBF_WINDOW_SIZE 20

#include <stdint.h> /* uint16_t */

struct msg_vec;
struct snake_id_vec;

struct server_client
{
    struct msg_vec*      pending_msgs;
    struct snake_id_vec* snakes_in_range;
    int                  timeout_counter;
    int      cbf_window[CBF_WINDOW_SIZE]; /* "Command Buffer Fullness" window */
    uint16_t snake_id;
    uint16_t last_command_msg_frame;
};
