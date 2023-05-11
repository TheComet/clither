#include "clither/args.h"
#include "clither/benchmarks.h"
#include "clither/cli_colors.h"
#include "clither/client.h"
#include "clither/controls.h"
#include "clither/gfx.h"
#include "clither/msg.h"
#include "clither/net.h"
#include "clither/log.h"
#include "clither/server.h"
#include "clither/signals.h"
#include "clither/tests.h"
#include "clither/tick.h"
#include "clither/world.h"

#include "cstructures/memory.h"

#if defined(_WIN32)
#   define _WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#else
#   include <unistd.h>
#   include <errno.h>
#   include <string.h>
#   include <sys/types.h>
#   include <sys/wait.h>
#endif

/* ------------------------------------------------------------------------- */
static void
run_server(const struct args* a)
{
    struct world world;
    struct server server;
    struct tick sim_tick;
    struct tick net_tick;
    uint16_t frame_number;

    /* Change log prefix and color for server log messages */
    log_set_prefix("Server: ");
    log_set_colors(COL_B_CYAN, COL_RESET);

    /* Install signal handlers for CTRL+C and (on windows) console close events */
    signals_install();

    world_init(&world);

    /* Init networking and bind server sockets */
    if (net_init() < 0)
        goto net_init_failed;
    if (server_init(&server, a->ip, a->port, a->config_file) < 0)
        goto net_init_connection_failed;
    net_log_host_ips();

    tick_cfg(&sim_tick, server.settings.sim_tick_rate);
    tick_cfg(&net_tick, server.settings.net_tick_rate);
    frame_number = 0;
    while (signals_exit_requested() == 0)
    {
        int tick_lag;
        int net_update = tick_advance(&net_tick);

        if (net_update)
            if (server_recv(&server, frame_number) != 0)
                break;

        /* sim_update */

        if (net_update)
            server_send_pending_data(&server);

        if ((tick_lag = tick_wait(&sim_tick)) > 0)
            log_warn("Server is lagging! Behind by %d tick%c\n", tick_lag, tick_lag > 1 ? 's' : ' ');

        frame_number++;
    }
    log_info("Stopping server\n");

    server_deinit(&server, a->config_file);
    net_deinit();
    world_deinit(&world);
    signals_remove();

    return;

net_init_connection_failed:
    net_deinit();
net_init_failed:
    signals_remove();
}

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_GFX)
static void
run_client(const struct args* a)
{
    struct world world;
    struct controls controls = { 0 };
    struct gfx* gfx;
    struct client client;
    struct tick sim_tick;
    struct tick net_tick;
    int tick_lag;

    /* Change log prefix and color for server log messages */
    log_set_prefix("Client: ");
    log_set_colors(COL_B_GREEN, COL_RESET);

    /* Install signal handlers for CTRL+C and (on windows) console close events */
#if !defined(_WIN32)
    signals_install();
#endif

    world_init(&world);

    /* Init all graphics and create window */
    if (gfx_init() < 0)
        goto init_gfx_failed;
    gfx = gfx_create(800, 600);
    if (gfx == NULL)
        goto create_gfx_failed;

    /* Init global networking */
    if (net_init() < 0)
        goto net_init_failed;
    client_init(&client);

    /* 
     * TODO: In the future the GUI will take care of connecting. Here we do
     * it immediately because there is no menu.
     */
    if (client_connect(&client, a->ip, a->port, "username") < 0)
        goto net_init_connection_failed;

    log_info("Client started\n");

    tick_cfg(&sim_tick, client.sim_tick_rate);  
    tick_cfg(&net_tick, client.net_tick_rate);
    while (1)
    {
        int net_update;

        gfx_poll_input(gfx, &controls);
        if (controls.quit)
            break;
#if !defined(_WIN32)
        if (signals_exit_requested())
            break;
#endif

        /* Receive net data */
        net_update = tick_advance(&net_tick);
        if (net_update && client.state != CLIENT_DISCONNECTED)
        {
            int action = client_recv(&client);

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

        if (net_update && client.state != CLIENT_DISCONNECTED)
        {
            if (client_send_pending_data(&client) != 0)
                break;
        }

        /* 
         * Skip rendering if we are lagging, as this is most likely the source
         * of the delay. If for some reason we end up 3 seconds behind where we
         * should be, quit.
         */
        tick_lag = tick_wait(&sim_tick);
        if (tick_lag == 0)
            gfx_update(gfx);
        else
        {
            log_dbg("Client is lagging! %d frames behind\n", tick_lag);
            if (tick_lag > client.sim_tick_rate * 3)  /* 3 seconds */
            {
                log_err("Client lagged too hard\n");
                break;
            }
        }

        client.frame_number++;
    }

    log_info("Stopping client\n");
    client_deinit(&client);
net_init_connection_failed:
    net_deinit();
net_init_failed:
    gfx_destroy(gfx);
    world_deinit(&world);
create_gfx_failed:
    gfx_deinit();
init_gfx_failed:
#if !defined(_WIN32)
    signals_remove();
#endif
    log_set_colors("", "");
    log_set_prefix("");
}
#endif

/* ------------------------------------------------------------------------- */
static uint64_t
start_background_server(const char* prog_name, const struct args* a)
{
#if defined(_WIN32)
    uint64_t ret;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    LPCSTR lpApplicationName = prog_name;
    LPSTR lpCommandLine = args_to_string(prog_name, a);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    ret = CreateProcess(
        lpApplicationName,
        lpCommandLine,
        NULL,
        NULL,
        FALSE,
        NORMAL_PRIORITY_CLASS | /* CREATE_NEW_CONSOLE | */ CREATE_NEW_PROCESS_GROUP,
        NULL,
        NULL,
        &si,
        &pi);
    args_free_string(lpCommandLine);

    if (!ret)
    {
        log_err("Failed to create server process\n");
        return (uint64_t)-1;
    }

    /* Windows handles fit into 32-bit integers. We can pack both here */
    ret = (uint64_t)pi.hProcess & 0xFFFFFFFF;
    ret |= ((uint64_t)pi.hThread & 0xFFFFFFFF) << 32;
    return ret;

#else
    pid_t pid = fork();
    if (pid < 0)
    {
        log_err("Failed to fork server process: %s\n", strerror(errno));
        return (uint64_t)-1;
    }
    if (pid == 0)
    {
        /* Change to a unique process group so ctrl+c only signals the client
         * process */
        setpgid(0, 0);
        run_server(a);
        return 0;
    }

    log_dbg("Forked server process %d\n", pid);
    return (uint64_t)pid;
#endif
}

/* ------------------------------------------------------------------------- */
static void
wait_for_background_server(uint64_t pid)
{
#if defined(_WIN32)
    HANDLE hProcess = (HANDLE)(pid & 0xFFFFFFFF);
    HANDLE hThread = (HANDLE)((pid >> 32) & 0xFFFFFFFF);
    WaitForSingleObject(hProcess, INFINITE);
    CloseHandle(hProcess);
    CloseHandle(hThread);
#else
    int status;
    signals_install();
    while (signals_exit_requested() == 0)
        pause();
    signals_remove();

    log_dbg("Sending SIGINT to server process %d\n", (int)pid);
    kill((pid_t)pid, SIGINT);
    if (waitpid((pid_t)pid, &status, 0) < 0)
    {
        log_warn("Server process is not responding, sending SIGTERM\n");
        kill((pid_t)pid, SIGTERM);
        if (waitpid((pid_t)pid, &status, 0) < 0)
        {
            log_warn("Server process is still not responding, sending SIGKILL\n");
            kill((pid_t)pid, SIGKILL);
        }
    }
#endif
}

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

    /* Open log file */
#if defined(CLITHER_LOGGING)
    if (*args.log_file)
        log_file_open(args.log_file);
#endif

    /*
     * cstructures global init. This is mostly for memory tracking during
     * debug builds
     */
    if (memory_init_thread() < 0)
    {
        log_err("Failed to initialize c structures library\n");
        goto cstructures_init_failed;
    }

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
        case MODE_HEADLESS:
            run_server(&args);
            break;
#if defined(CLITHER_GFX)
        case MODE_CLIENT:
            run_client(&args);
            break;
        case MODE_CLIENT_AND_SERVER: {
            uint64_t subprocess;
            struct args server_args = args;

            /* Modify the command line arguments for client and server */
            server_args.mode = MODE_HEADLESS;
#if defined(CLITHER_LOGGING)
            server_args.log_file = "";
#endif
            args.ip = "127.0.0.1";

            subprocess = start_background_server(argv[0], &server_args);
            if (subprocess == (uint64_t)-1) /* error */
                break;
            /*
             * On linux, the forked server process will return when it stops.
             * This is indicated by returning 0 (on Windows, the server process
             * starts from main() instead). We still have a log file to close
             * in this situation.
             */
            if (subprocess == 0)
                break;

            /* The server should be running, so try to join as a client */
            run_client(&args);

            log_note("The server will continue to run.\n");
            log_note("You can stop it by pressing CTRL+C\n");
            wait_for_background_server(subprocess);
        } break;
#endif
    }

    memory_deinit_thread();
#if defined(CLITHER_LOGGING)
    log_file_close();
#endif

    return retval;

#if defined(CLITHER_LOGGING)
    cstructures_init_failed : log_file_close();
#else
    cstructures_init_failed : ;
#endif
    return -1;
}

/* ------------------------------------------------------------------------- */
#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    return main(__argc, __argv);
}
#endif
