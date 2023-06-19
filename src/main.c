#include "clither/args.h"
#include "clither/benchmarks.h"
#include "clither/camera.h"
#include "clither/cli_colors.h"
#include "clither/client.h"
#include "clither/command.h"
#include "clither/gfx.h"
#include "clither/input.h"
#include "clither/log.h"
#include "clither/mcd_wifi.h"
#include "clither/msg.h"
#include "clither/mutex.h"
#include "clither/net.h"
#include "clither/resource_pack.h"
#include "clither/server.h"
#include "clither/server_settings.h"
#include "clither/signals.h"
#include "clither/snake.h"
#include "clither/tests.h"
#include "clither/thread.h"
#include "clither/tick.h"
#include "clither/world.h"

#include "cstructures/memory.h"
#include "cstructures/btree.h"

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

    memory_init_thread();

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
                snake_step(&snake->data, &snake->head, &snake->param, command, instance->settings->sim_tick_rate);
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

    memory_deinit_thread();

    return (void*)0;

server_init_failed:
    world_deinit(&world);
    log_set_colors("", "");
    log_set_prefix("");
    return (void*)-1;
}

/* ------------------------------------------------------------------------- */
static void*
run_server(const void* args)
{
#define INSTANCE_FOR_EACH(btree, port, instance) \
    BTREE_FOR_EACH(btree, struct server_instance, port, instance)
#define INSTANCE_END_EACH \
    BTREE_END_EACH

    struct cs_btree instances;
    struct server_settings settings;
    const struct args* a = args;

    /* Change log prefix and color for server log messages */
    log_set_prefix("Server: ");
    log_set_colors(COL_B_CYAN, COL_RESET);

    memory_init_thread();

    btree_init(&instances, sizeof(struct server_instance));

    if (server_settings_load_or_set_defaults(&settings, a->config_file) < 0)
        goto load_settings_failed;

    /*
     * Create the default server instance. This is always active, regardless of
     * how many players are connected.
     */
    {
        /*
         * The port passed in over the command line has precedence over the port
         * specified in the config file. Note that the port obtained from the
         * settings structure is always initialized, regardless of whether the
         * config file existed or not.
         */
        struct server_instance* instance;
        const char* port = *a->port ? a->port : settings.port;
        cs_btree_key key = atoi(port);
        assert(key != 0);

        instance = btree_emplace_new(&instances, key);
        assert(instance != NULL);
        instance->settings = &settings;
        instance->ip = a->ip;
        strcpy(instance->port, port);

        log_dbg("Starting default server instance\n");
        if (thread_start(&instance->thread, run_server_instance, instance) < 0)
        {
            log_err("Failed to start the default server instance! Can't continue\n");
            goto start_default_instance_failed;
        }
    }

    /* For now we don't create more instances once the server fills up */
    INSTANCE_FOR_EACH(&instances, port, instance)
        thread_join(instance->thread, 0);
    INSTANCE_END_EACH
    log_dbg("Joined all server instances\n");

    server_settings_save(&settings, a->config_file);

    btree_deinit(&instances);
    memory_deinit_thread();
    log_set_colors("", "");
    log_set_prefix("");

    return (void*)0;

start_default_instance_failed:
load_settings_failed:
    btree_deinit(&instances);
    memory_deinit_thread();
    log_set_colors("", "");
    log_set_prefix("");
    return (void*)-1;
}

/* ------------------------------------------------------------------------- */
static int
start_background_server(struct thread* t, const struct args* a)
{
    log_dbg("Starting server in background thread\n");
    if (thread_start(t, run_server, a) < 0)
        return -1;

    /*
     * Wait (for a reasonable amount of time) until the server reports it is
     * running, or until it fails. This is so we don't start the client if
     * something goes wrong, but also ensures that the server socket is bound
     * before the client tries to join.
     */
    /* TODO */
    return 0;
}

/* ------------------------------------------------------------------------- */
static void
join_background_server(struct thread t)
{
    thread_join(t, 0);
    log_dbg("Joined background server thread\n");
}
#endif  /* CLITHER_SERVER */

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_GFX)
static void
run_client(const struct args* a)
{
#if defined(CLITHER_MCD)
    struct thread mcd_thread;
#endif
    struct world world;
    struct input input;
    struct command command;
    const struct gfx_interface* gfx_iface;
    struct gfx* gfx;
    struct resource_pack* pack;
    struct camera camera;
    struct client client;
    struct tick sim_tick;
    struct tick net_tick;
    int tick_lag;

    /* Change log prefix and color for server log messages */
    log_set_prefix("Client: ");
    log_set_colors(COL_B_GREEN, COL_RESET);

    memory_init_thread();

    /* If McDonald's WiFi is enabled, start that */
    client_init(&client);
#if defined(CLITHER_MCD)
    if (a->mcd_latency > 0)
    {
        if (thread_start(&mcd_thread, run_mcd_wifi, a) < 0)
            goto start_mcd_failed;
        if (client_connect(&client, a->ip, a->mcd_port, "username") < 0)
            goto client_connect_failed;
    }
    else
#endif
    {
        /*
         * TODO: In the future the GUI will take care of connecting. Here we do
         * it immediately because there is no menu.
         */
        if (client_connect(&client, a->ip, a->port, "username") < 0)
            goto client_connect_failed;
    }

    /* Init all graphics and create window */
    gfx_iface = gfx_backends[a->gfx_backend];
    log_info("Using graphics backend: %s\n", gfx_iface->name);
    if (gfx_iface->global_init() < 0)
        goto init_gfx_failed;
    gfx = gfx_iface->create(800, 600);
    if (gfx == NULL)
        goto create_gfx_failed;

    pack = resource_pack_parse("packs/horror");
    if (pack == NULL)
        goto parse_resource_pack_failed;
    if (gfx_iface->load_resource_pack(gfx, pack) < 0)
        goto load_resource_pack_failed;

    input_init(&input);
    camera_init(&camera);
    command = command_default();
    world_init(&world);

    log_info("Client started\n");

    tick_cfg(&sim_tick, client.sim_tick_rate);
    tick_cfg(&net_tick, client.net_tick_rate);
    while (signals_exit_requested() == 0)
    {
        int net_update;

        gfx_iface->poll_input(gfx, &input);
        if (input.quit)
            break;

        /* Switch graphics backends */
        if (input.next_gfx_backend || input.prev_gfx_backend)
        {
            int count;
            int idx, new_idx;

            for (count = 0; gfx_backends[count]; ++count)
            {}

            for (idx = 0; gfx_backends[idx]; ++idx)
                if (gfx_iface == gfx_backends[idx])
                    break;

            if (input.next_gfx_backend)
                new_idx = idx + 1;
            if (input.prev_gfx_backend)
                new_idx = idx - 1;
            if (new_idx >= count)
                new_idx = 0;
            if (new_idx < 0)
                new_idx = count - 1;

            gfx_iface->destroy(gfx);
            gfx_iface->global_deinit();

            gfx_iface = gfx_backends[new_idx];
            if (gfx_iface->global_init() < 0)
                goto init_new_gfx_failed;

            gfx = gfx_iface->create(640, 480);
            if (gfx == NULL)
                goto create_new_gfx_failed;

            if (gfx_iface->load_resource_pack(gfx, pack) < 0)
                goto load_new_resource_pack_failed;

            input_init(&input);
            gfx_iface->poll_input(gfx, &input);

            goto create_new_gfx_success;

        load_new_resource_pack_failed : gfx_iface->destroy(gfx);
        create_new_gfx_failed         : gfx_iface->global_deinit();
        init_new_gfx_failed           :
            /* Jesus */
            gfx_iface = gfx_backends[idx];
            if (gfx_iface->global_init() < 0)
                break;
            gfx = gfx_iface->create(640, 480);
            if (gfx == NULL)
            {
                gfx_iface->global_deinit();
                break;
            }
        } create_new_gfx_success:;

        /* Receive net data */
        net_update = tick_advance(&net_tick);
        if (net_update && client.state != CLIENT_DISCONNECTED)
        {
            int action = client_recv(&client, &world);

            /* Some error occurred */
            if (action == -1)
                break;

            if (action == 1)
            {
                /*
                 * We may have to match our tick rates to the server, because
                 * the server can freely configure these values. If the client
                 * disconnected then sim_tick_rate and net_tick_rate are reset
                 * to their default values, so in this case we also want to
                 * update the tick rate.
                 */
                tick_cfg(&sim_tick, client.sim_tick_rate);
                tick_cfg(&net_tick, client.net_tick_rate);
                log_dbg("Sim tick rate: %d, net tick rate: %d\n", client.sim_tick_rate, client.net_tick_rate);
            }
        }

        /* sim_update */
        if (client.state == CLIENT_CONNECTED)
        {
            struct snake* snake = world_get_snake(&world, client.snake_id);

            /*
             * Map "input" to "command". This converts the mouse and keyboard
             * information into a structure that lets us step the snake forwards
             * in time.
             */
            command = gfx_iface->input_to_command(command, &input, gfx, &camera, snake->head.pos);

            /*
             * Append the new command to the ring buffer of unconfirmed commands.
             * This entire list is sent to the server every network update so
             * in the event of packet loss, the server always has a complete
             * history of what our snake has done, frame by frame. When the server
             * acknowledges our move, we remove all commands that date back before
             * and up to that point in time from the list again.
             */
            command_rb_put(&snake->command_rb, command, client.frame_number);

            /* Update snake and step */
            snake_param_update(&snake->param, snake->param.upgrades, snake->param.food_eaten + 1);
            snake_step(&snake->data, &snake->head, &snake->param, command, client.sim_tick_rate);
            world_step(&world, client.frame_number, client.sim_tick_rate);

            camera_update(&camera, &snake->head, &snake->param, client.sim_tick_rate);

            if (net_update)
            {
                /* Send all unconfirmed commands (unreliable) */
                msg_commands(&client.pending_msgs, &snake->command_rb);
            }
        }

        if (net_update && client.state != CLIENT_DISCONNECTED)
        {
            if (client_send_pending_data(&client) < 0)
                break;
        }

        gfx_iface->step_anim(gfx, client.sim_tick_rate);

        /*
         * Skip rendering if we are lagging, as this is most likely the source
         * of the delay. If for some reason we end up 3 seconds behind where we
         * should be, quit.
         */
        tick_lag = tick_wait_warp(&sim_tick, client.warp, client.sim_tick_rate * 10);
        if (tick_lag == 0)
            gfx_iface->draw_world(gfx, &world, &camera);
        else
        {
            log_dbg("Client is lagging! %d frames behind\n", tick_lag);
            if (tick_lag > client.sim_tick_rate * 3)  /* 3 seconds */
            {
                tick_skip(&sim_tick);
                break;
            }
        }

        if (client.warp > 0)
            client.warp--;
        if (client.warp < 0)
            client.warp++;

        client.frame_number++;
    }
    log_info("Stopping client\n");

    world_deinit(&world);
load_resource_pack_failed:
    resource_pack_destroy(pack);
parse_resource_pack_failed:
    gfx_iface->destroy(gfx);
create_gfx_failed:
    gfx_iface->global_deinit();
init_gfx_failed:
    client_disconnect(&client);
client_connect_failed:
    /* Stop McDonald's WiFi if necessary */
#if defined(CLITHER_MCD)
    if (a->mcd_latency > 0)
    {
        thread_join(mcd_thread, 0);
        log_dbg("Joined McDonald's WiFi thread\n");
    }
start_mcd_failed:
#endif
    client_deinit(&client);
    memory_deinit_thread();
    log_set_colors("", "");
    log_set_prefix("");
}
#endif

/* ------------------------------------------------------------------------- */
int main(int argc, char* argv[])
{
    int retval;

    /*
     * Parse command line args before doing anything else. This function
     * returns -1 if an error occurred, 0 if we can continue running, and 1
     * if --help appeared, in which case we should exit.
     */
    struct args args;
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
            retval = tests_run(argc, argv);
            break;
#endif
#if defined(CLITHER_BENCHMARKS)
        case MODE_BENCHMARKS:
            retval = benchmarks_run(argc, argv);
            break;
#endif
#if defined(CLITHER_SERVER)
        case MODE_HEADLESS: {
            retval = (int)(intptr_t)run_server(&args);
        } break;
#endif
#if defined(CLITHER_GFX)
        case MODE_CLIENT:
            run_client(&args);
            break;
#endif
#if defined(CLITHER_GFX) && defined(CLITHER_SERVER)
        case MODE_CLIENT_AND_SERVER: {
            struct thread server_thread;
            struct args server_args = args;

            if (start_background_server(&server_thread, &server_args) < 0)
            {
                retval = -1;
                break;
            }

            /* The server should be running, so try to join as a client */
            args.ip = "localhost";
            run_client(&args);

            if (!signals_exit_requested())
            {
                log_note("The server will continue to run.\n");
                log_note("You can stop it by pressing CTRL+C\n");
            }
            join_background_server(server_thread);
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

    return retval;

net_init_failed:
#if defined(CLITHER_LOGGING)
    log_file_close();
#endif
    signals_remove();
    return -1;
}
