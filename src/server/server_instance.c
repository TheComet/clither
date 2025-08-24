#include "clither/game/leaderboard.h"
#include "clither/game/settings.h"
#include "clither/game/snake_bmap.h"
#include "clither/game/world.h"
#include "clither/platform/net.h"
#include "clither/platform/semaphore.h"
#include "clither/platform/signals.h"
#include "clither/platform/thread.h"
#include "clither/platform/tick.h"
#include "clither/server/server.h"
#include "clither/server/server_instance.h"
#include "clither/util/bmap.h"
#include "clither/util/cli_colors.h"
#include "clither/util/log.h"
#include "clither/util/tracker.h"
#include <stdio.h>  /* sprintf */
#include <stdlib.h> /* atoi */

/* ------------------------------------------------------------------------- */
static void* server_instance_run(const void* arg)
{
    struct world                  world;
    struct leaderboard            leaderboard;
    struct server                 server;
    struct tick                   sim_tick;
    struct tick                   net_tick;
    uint16_t                      frame_number;
    uint8_t                       leaderboard_update_counter;
    char                          log_prefix[] = "S:xxxxx ";
    const struct server_instance* instance = arg;

    static const char* colors[] = {
        COL_N_CYAN, COL_N_MAGENTA, COL_N_BLUE, COL_N_GREEN, COL_N_RED};

    if (trackers_init_tls() != 0)
        goto mem_init_failed;
    log_init();

    /* Change log prefix and color for server log messages */
    sprintf(log_prefix + 2, "%-6s", instance->port);
    log_set_prefix(log_prefix);
    log_set_colors(colors[atoi(instance->port) % 5], COL_RESET);

    world_init(&world);
    world_update_settings(&world, &instance->settings->world);
    if (world_respawn_food(&world) != 0)
        goto world_spawn_food_failed;

    leaderboard_init(&leaderboard);

    if (server_init(&server, instance->addr, instance->port) != 0)
        goto server_init_failed;
    net_log_host_ips();

    semaphore_give(instance->ready);

    log_dbg("Started server instance\n");
    tick_cfg(&sim_tick, instance->settings->server.sim_tick_rate);
    tick_cfg(&net_tick, instance->settings->server.net_tick_rate);
    frame_number = 0;
    leaderboard_update_counter = 0;
    while (1)
    {
        struct snake* snake;
        int16_t       idx;
        int           tick_lag, net_update;
        uint16_t      uid;

        if (signals_exit_requested())
            break;
        if (semaphore_try_take(instance->stop))
            break;

        net_update = tick_advance(&net_tick);
        if (server_recv(
                &server,
                &instance->settings->server,
                &world,
                &instance->settings->world,
                frame_number) != 0)
        {
            break;
        }

        /* sim_update */
        bmap_for_each (world.snakes, idx, uid, snake)
        {
            struct cmd cmd;
            (void)uid;
            if (!snake_try_reset_hold(snake, frame_number))
                continue;
            if (snake_is_dead(snake))
                continue;

            cmd = cmd_queue_take_or_predict(&snake->cmdq, frame_number);
            snake_eat_food(&snake->head, &snake->param, world.food_bmap);
            snake_remove_stale_segments(
                &snake->data,
                snake_step(
                    &snake->data,
                    &snake->head,
                    &snake->param,
                    cmd,
                    instance->settings->server.sim_tick_rate));
            if (server_update_snakes_in_range(&server, &world) != 0)
                break;
            if (server_kill_snake_checks(&server, &world) != 0)
                break;
        }

        if (world_respawn_food(&world) != 0)
            break;

        if (net_update)
        {
            if (server_queue_snake_data(&server, &world, frame_number) != 0)
                break;
            if (server_queue_food_data(&server, &world) != 0)
                break;

            if (leaderboard_update_counter++ ==
                instance->settings->server.net_tick_rate)
            {
                leaderboard_update_counter = 0;
                leaderboard_update(&leaderboard, &world);
                if (server_queue_leaderboard(&server, &leaderboard) != 0)
                    break;
            }

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
    leaderboard_deinit(&leaderboard);
    world_deinit(&world);

    trackers_deinit_tls();

    return (void*)0;

server_init_failed:
    leaderboard_deinit(&leaderboard);
world_spawn_food_failed:
    world_deinit(&world);
    trackers_deinit_tls();
mem_init_failed:
    return (void*)-1;
}

/* ------------------------------------------------------------------------- */
void server_instance_init(struct server_instance* instance)
{
    instance->settings = NULL;
    instance->addr = NULL;
    instance->port = NULL;
    instance->thread = NULL;
    instance->ready = NULL;
    instance->stop = NULL;
    instance->thread = NULL;
}

/* ------------------------------------------------------------------------- */
int server_instance_is_running(const struct server_instance* instance)
{
    return instance->thread != NULL;
}

/* ------------------------------------------------------------------------- */
int server_instance_start(
    struct server_instance* instance,
    const struct settings*  settings,
    const char*             addr,
    const char*             port)
{
    instance->settings = settings;
    instance->addr = addr;
    instance->port = port;
    instance->thread = NULL;

    instance->ready = semaphore_create(0);
    if (instance->ready == NULL)
        goto alloc_ready_failed;

    instance->stop = semaphore_create(0);
    if (instance->stop == NULL)
        goto alloc_stop_failed;

    instance->thread = thread_start(server_instance_run, instance);
    if (instance->thread == NULL)
        goto start_thread_failed;

    return 0;

start_thread_failed:
    semaphore_destroy(instance->stop);
alloc_stop_failed:
    semaphore_destroy(instance->ready);
alloc_ready_failed:
    return -1;
}

/* ------------------------------------------------------------------------- */
void server_instance_wait_for_ready(struct server_instance* instance)
{
    semaphore_take(instance->ready);
}

/* ------------------------------------------------------------------------- */
void server_instance_wait_for_finish(struct server_instance* instance)
{
    thread_join(instance->thread);
    semaphore_destroy(instance->stop);
    semaphore_destroy(instance->ready);

    instance->thread = NULL;
}

/* ------------------------------------------------------------------------- */
void server_instance_stop(struct server_instance* instance)
{
    semaphore_give(instance->stop);
    server_instance_wait_for_finish(instance);
}
