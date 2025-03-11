#pragma once

#define CBF_WINDOW_SIZE 20

#include <stdint.h> /* int16_t, uint16_t */

struct server_client
{
    struct msg_vec* pending_msgs;
    int             timeout_counter;
    int      cbf_window[CBF_WINDOW_SIZE]; /* "Command Buffer Fullness" window */
    int16_t  snake_id;
    uint16_t last_command_msg_frame;
};
