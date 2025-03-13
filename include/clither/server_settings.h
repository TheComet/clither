#pragma once

#include "clither/config.h"
#include "clither/hm.h"

struct server_settings
{
    uint16_t max_players;
    uint16_t client_timeout;
    uint16_t malicious_timeout;
    uint8_t  max_username_len;
    uint8_t  sim_tick_rate;
    uint8_t  net_tick_rate;
    char     port[6];

    /*struct cs_hashmap banned_ips;*/
};

void server_settings_set_defaults(struct server_settings* s);

int server_settings_load_or_set_defaults(
    struct server_settings* s, const char* filename);

void server_settings_save(
    const struct server_settings* s, const char* filename);
