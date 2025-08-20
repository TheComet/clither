#pragma once

#include "clither/game/settings_ini.h"

struct args;

SECTION("server")
struct settings_server
{
    uint16_t max_players       DEFAULT(600) CONSTRAIN(1, 0xFFFF);
    uint16_t client_timeout    DEFAULT(5);
    uint16_t malicious_timeout DEFAULT(60);
    uint8_t max_username_len   DEFAULT(32);
    uint8_t sim_tick_rate      DEFAULT(60) CONSTRAIN(1, 255);
    uint8_t net_tick_rate      DEFAULT(20) CONSTRAIN(1, 255);
    /* Can be a URL, hence the buffer size */
    char bind_addr[256];
    char bind_port[6] DEFAULT("6666");
    char log_prefix[32] DEFAULT("Server: ");
};

SECTION("world")
struct settings_world
{
    uint32_t food_count  DEFAULT(10000) CONSTRAIN(0, 100000000);
    uint8_t inner_radius DEFAULT(120) CONSTRAIN(5, 255);
    uint8_t ring_start   DEFAULT(190) CONSTRAIN(5, 255);
    uint8_t ring_end     DEFAULT(255) CONSTRAIN(5, 255);
};

SECTION("client")
struct settings_client
{
    char username[256] DEFAULT("Snek: D");
    /* Can be a URL, hence the buffer size */
    char connect_addr[256] DEFAULT("localhost");
    char connect_port[6] DEFAULT("5555");
    char log_prefix[32] DEFAULT("Client: ");
};

SECTION("gfx")
struct settings_gfx
{
    int width  DEFAULT(1280) CONSTRAIN(64, 65535);
    int height DEFAULT(960) CONSTRAIN(64, 65535);
    uint8_t    backend;
    unsigned   enable : 1 DEFAULT(1);
    unsigned   fullscreen : 1;
};

SECTION("mcd")
struct settings_mcd
{
    int latency_ms      DEFAULT(400) CONSTRAIN(0, 4000);
    int loss_percent    DEFAULT(20) CONSTRAIN(0, 1000);
    int dup_percent     DEFAULT(20) CONSTRAIN(0, 1000);
    int reorder_percent DEFAULT(20) CONSTRAIN(0, 1000);

    char     bind_addr[256];
    char     bind_port[6] DEFAULT("5554");
    char     connect_addr[256] DEFAULT("localhost");
    char     connect_port[6] DEFAULT("5555");
    unsigned enable : 1;
};

#define SNAKE_COSMETIC_PARAMS_LIST                                             \
    X(part_spacing, PART_SPACING, 0.32, 0.1, 0.8)                              \
    X(spine_width, SPINE_WIDTH, 0.11, 0.05, 0.3)                               \
    X(head_scale, HEAD_SCALE, 0.25, 0.2, 0.3)                                  \
    X(body_scale, BODY_SCALE, 0.25, 0.2, 0.3)                                  \
    X(tail_scale, TAIL_SCALE, 0.25, 0.2, 0.3)                                  \
    X(girth, GIRTH, 0.0, 0.0, 1.0)                                             \
    X(decay, DECAY, 0.0, 0.0, 1.0)

SECTION("snake")
struct settings_snake
{
    float part_spacing DEFAULT(0.32) CONSTRAIN(0.1, 0.8);
    float spine_width  DEFAULT(0.11) CONSTRAIN(0.05, 0.3);
    float head_scale   DEFAULT(0.25) CONSTRAIN(0.2, 0.3);
    float body_scale   DEFAULT(0.25) CONSTRAIN(0.2, 0.3);
    float tail_scale   DEFAULT(0.25) CONSTRAIN(0.2, 0.3);
    float girth        DEFAULT(0.0) CONSTRAIN(0.0, 1.0);
    float decay        DEFAULT(0.0) CONSTRAIN(0.0, 1.0);
};

#define SETTINGS_SECTIONS_LIST                                                 \
    X(server)                                                                  \
    X(world)                                                                   \
    X(client)                                                                  \
    X(gfx)                                                                     \
    X(mcd)                                                                     \
    X(snake)

struct settings
{
#define X(sec) struct settings_##sec sec;
    SETTINGS_SECTIONS_LIST
#undef X
};

void settings_init(struct settings* s);
void settings_deinit(struct settings* s);
int  settings_load(struct settings* s, const char* filename);
void settings_save(const struct settings* s, const char* filename);
int  settings_apply_args(struct settings* s, const struct args* a);
