#include "clither/bmap.h"
#include "clither/cli_colors.h"
#include "clither/log.h"
#include "clither/mem.h"
#include "clither/net.h"
#include "clither/server.h"
#include "clither/server_instance.h"
#include "clither/server_settings.h"
#include "clither/signals.h"
#include "clither/snake_bmap.h"
#include "clither/tick.h"
#include "clither/world.h"
#include <stdio.h>  /* sprintf */
#include <stdlib.h> /* atoi */

/* ------------------------------------------------------------------------- */
void* server_instance_run(const void* args)
{
    struct world                  world;
    struct server                 server;
    struct tick                   sim_tick;
    struct tick                   net_tick;
    uint16_t                      frame_number;
    char                          log_prefix[] = "S:xxxxx ";
    const struct server_instance* instance = args;

    static const char* colors[] = {
        COL_N_CYAN, COL_N_MAGENTA, COL_N_BLUE, COL_N_GREEN, COL_N_RED};

    mem_init_threadlocal();

    /* Change log prefix and color for server log messages */
    sprintf(log_prefix + 2, "%-6s", instance->port);
    log_set_prefix(log_prefix);
    log_set_colors(colors[atoi(instance->port) % 5], COL_RESET);

    world_init(&world);

    if (server_init(&server, instance->ip, instance->port) < 0)
        goto server_init_failed;
    net_log_host_ips();

    log_dbg("Started server instance\n");
    tick_cfg(&sim_tick, instance->settings->sim_tick_rate);
    tick_cfg(&net_tick, instance->settings->net_tick_rate);
    frame_number = 0;
    while (signals_exit_requested() == 0)
    {
        struct snake* snake;
        int16_t       idx;
        int           tick_lag, net_update;
        uint16_t      uid;

        net_update = tick_advance(&net_tick);
        if (net_update)
        {
            if (server_recv(
                    &server, instance->settings, &world, frame_number) != 0)
                break;
        }

        /* sim_update */
        bmap_for_each (world.snakes, idx, uid, snake)
        {
            struct cmd cmd;
            (void)uid;
            if (!snake_try_reset_hold(snake, frame_number))
                continue;
            cmd = cmd_queue_take_or_predict(&snake->cmdq, frame_number);
            /*snake_param_update(
                 &snake->param,
                 snake->param.upgrades,
                 snake->param.food_eaten + 1);*/
            snake_remove_stale_segments(
                &snake->data,
                snake_step(
                    &snake->data,
                    &snake->head,
                    &snake->param,
                    cmd,
                    instance->settings->sim_tick_rate));
        }
        world_step(&world, frame_number, instance->settings->sim_tick_rate);

        if (net_update)
        {
            if (server_update_snakes_in_range(&server, &world, make_qw(10)) !=
                0)
                break;
            if (server_queue_snake_data(&server, &world, frame_number) != 0)
                break;
            if (server_send_pending_data(&server, &world) != 0)
                break;
        }

        if ((tick_lag = tick_wait(&sim_tick)) > 0)
            log_warn(
                "Server is lagging! Behind by %d tick%c\n",
                tick_lag,
                tick_lag == 1 ? ' ' : 's');

        frame_number++;
    }
    log_info("Stopping server instance\n");

    server_deinit(&server);
    world_deinit(&world);

    (void)mem_deinit_threadlocal();

    return (void*)0;

server_init_failed:
    world_deinit(&world);
    log_set_colors("", "");
    log_set_prefix("");
    return (void*)-1;
}
