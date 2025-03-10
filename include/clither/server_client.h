#pragma once

#include "cstructures/btree.h"

#define CBF_WINDOW_SIZE 20

struct server_client
{
    struct msg_vec* pending_msgs;
    int             timeout_counter;
    int cbf_window[CBF_WINDOW_SIZE]; /* "Command Buffer Fullness" window */
    cs_btree_key snake_id;
    uint16_t     last_command_msg_frame;
};

