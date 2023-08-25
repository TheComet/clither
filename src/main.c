#include "clither/args.h"
#include "clither/benchmarks.h"
#include "clither/cli_colors.h"
#include "clither/client.h"
#include "clither/command.h"
#include "clither/log.h"
#include "clither/mutex.h"
#include "clither/net.h"
#include "clither/server.h"
#include "clither/server_settings.h"
#include "clither/signals.h"
#include "clither/snake.h"
#include "clither/tests.h"
#include "clither/thread.h"
#include "clither/tick.h"
#include "clither/world.h"

#include "cstructures/btree.h"
#include "cstructures/init.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#if defined(CLITHER_SERVER)

static int server_instances = 0;
static struct mutex server_mutex;

struct server_instance
{
    struct thread thread;
    const struct server_settings* settings;
    const char* ip;
    char port[6];
};

/* ------------------------------------------------------------------------- */
static void*
run_server_instance(const void* args)
{
    struct world world;
    struct server server;
    struct tick sim_tick;
    struct tick net_tick;
    uint16_t frame_number;
    char log_prefix[] = "S:xxxxx ";
    const struct server_instance* instance = args;

    static const char* colors[] = {
        COL_N_CYAN,
        COL_N_MAGENTA,
        COL_N_BLUE,
        COL_N_GREEN,
        COL_N_RED
    };

    /* Change log prefix and color for server log messages */
    sprintf(log_prefix+2, "%-6s", instance->port);
    log_set_prefix(log_prefix);
    log_set_colors(colors[atoi(instance->port) % 5], COL_RESET);

    cs_threadlocal_init();

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
        int tick_lag;
        int net_update = tick_advance(&net_tick);

        if (net_update)
        {
            if (server_recv(&server, instance->settings, &world, frame_number) != 0)
                break;
        }

        /* sim_update */
        WORLD_FOR_EACH_SNAKE(&world, uid, snake)
            if (!snake_is_held(snake, frame_number))
            {
                struct command command = command_rb_take_or_predict(&snake->command_rb, frame_number);
                snake_param_update(&snake->param, snake->param.upgrades, snake->param.food_eaten + 1);
                snake_remove_stale_segments(&snake->data,
                    snake_step(&snake->data, &snake->head, &snake->param, command, instance->settings->sim_tick_rate));
            }
        WORLD_END_EACH
        world_step(&world, frame_number, instance->settings->sim_tick_rate);

        if (net_update)
        {
            server_queue_snake_data(&server, &world, frame_number);
            server_send_pending_data(&server);
        }

        if ((tick_lag = tick_wait(&sim_tick)) > 0)
            log_warn("Server is lagging! Behind by %d tick%c\n", tick_lag, tick_lag == 1 ? ' ' : 's');

        frame_number++;
    }
    log_info("Stopping server instance\n");

    server_deinit(&server);
    world_deinit(&world);

    cs_threadlocal_deinit();

    return (void*)0;

server_init_failed:
    world_deinit(&world);
    log_set_colors("", "");
    log_set_prefix("");
    return (void*)-1;
}

/* ------------------------------------------------------------------------- */
int main(int argc, char* argv[])
{
    struct args args;
    int retval;

    /*
     * Parse command line args before doing anything else. This function
     * returns -1 if an error occurred, 0 if we can continue running, and 1
     * if --help appeared, in which case we should exit.
     */
    switch (args_parse(&args, argc, argv))
    {
        case 0: break;
        case 1: return 0;
        default: return -1;
    }

    /* Install signal handlers for CTRL+C and (on windows) console close events */
    signals_install();

    /* Open log file */
#if defined(CLITHER_LOGGING)
    if (*args.log_file)
        log_file_open(args.log_file);
    log_net_open("net.txt");
#endif

    /* Init networking */
    if (net_init() < 0)
        goto net_init_failed;

#if defined(CLITHER_SERVER)
    mutex_init(&server_mutex);
    server_instances = 0;
#endif

    retval = 0;
    switch (args.mode)
    {
#if defined(CLITHER_TESTS)
        case MODE_TESTS:
            if (cs_init() < 0)
            {
                log_err("Failed to initialized cstructures library\n");
                break;
            }
            retval = tests_run(argc, argv);
            cs_deinit();
            break;
#endif
#if defined(CLITHER_BENCHMARKS)
        case MODE_BENCHMARKS:
            if (cs_init() < 0)
            {
                log_err("Failed to initialized cstructures library\n");
                break;
            }
            retval = benchmarks_run(argc, argv);
            cs_deinit();
            break;
#endif
#if defined(CLITHER_SERVER)
        case MODE_HEADLESS:
            retval = (int)(intptr_t)server_run(&args);
            break;
#endif
#if defined(CLITHER_GFX)
        case MODE_CLIENT:
            retval = (int)(intptr_t)client_run(&args);
            break;
#endif
#if defined(CLITHER_GFX) && defined(CLITHER_SERVER)
        case MODE_CLIENT_AND_SERVER: {
            struct thread server_thread;
            struct args server_args = args;

            log_dbg("Starting server in background thread\n");
            if (thread_start(&server_thread, &server_run, &server_args) < 0)
            {
                retval = -1;
                break;
            }

            /* The server should be running, so try to join as a client */
            args.ip = "localhost";
            retval += (int)(intptr_t)run_client(&args);

            if (!signals_exit_requested())
            {
                log_note("The server will continue to run.\n");
                log_note("You can stop it by pressing CTRL+C\n");
            }

            retval += thread_join(server_thread, 0);
            log_dbg("Joined background server thread\n");
        } break;
#endif
    }

#if defined(CLITHER_SERVER)
    mutex_deinit(server_mutex);
#endif
    net_deinit();
#if defined(CLITHER_LOGGING)
    log_net_close();
    log_file_close();
#endif
    signals_remove();
    cs_deinit();

    return retval;

net_init_failed:
#if defined(CLITHER_LOGGING)
    log_file_close();
#endif
    signals_remove();
    cs_deinit();
cstructures_init_failed:
    return -1;
}
