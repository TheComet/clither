#pragma once

#include <stdint.h>

struct args;

struct settings_server
{
    uint16_t max_players;
    uint16_t client_timeout;
    uint16_t malicious_timeout;
    uint8_t  max_username_len;
    uint8_t  sim_tick_rate;
    uint8_t  net_tick_rate;
    char     bind_addr[256]; /* Can be a URL, hence the buffer size */
    char     bind_port[6];
    char     log_prefix[32];
};

struct settings_world
{
    uint32_t food_count;
    uint8_t  inner_radius;
    uint8_t  ring_start;
    uint8_t  ring_end;
};

struct settings_client
{
    char username[256];
    char connect_addr[256]; /* Can be a URL, hence the buffer size */
    char connect_port[6];
    char log_prefix[32];
};

struct settings_gfx
{
    int      width, height;
    uint8_t  backend;
    unsigned enable : 1;
    unsigned fullscreen : 1;
};

struct settings_mcd
{
    int      latency_ms;
    int      loss_percent;
    int      dup_percent;
    int      reorder_percent;
    char     bind_addr[256];
    char     bind_port[6];
    char     connect_addr[256];
    char     connect_port[6];
    unsigned enable : 1;
};

struct settings
{
    struct settings_server server;
    struct settings_world  world;
    struct settings_client client;
    struct settings_gfx    gfx;
    struct settings_mcd    mcd;
};

void settings_set_defaults(struct settings* s);
int  settings_apply_args(struct settings* s, const struct args* a);
int  settings_load(struct settings* s, const char* filename);
void settings_save(const struct settings* s, const char* filename);
