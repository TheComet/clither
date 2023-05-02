#include "clither/args.h"
#include "clither/cli_colors.h"
#include "clither/net.h"
#include "clither/log.h"

#if defined(_WIN32)
#   define _WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#else
#   include <unistd.h>
#   include <errno.h>
#   include <string.h>
#   include <signal.h>
#   include <sys/types.h>
#   include <sys/wait.h>
#endif

/* ------------------------------------------------------------------------- */
static volatile char ctrl_c_pressed = 0;
static void handle_ctrl_c(int sig)
{
    (void)sig;
    ctrl_c_pressed = 1;
}

/* ------------------------------------------------------------------------- */
static void
run_server(const struct args* a)
{
    struct sigaction act, old_act;
    struct net_sockets sockets;

    log_set_prefix("Server: ");
    log_set_colors(COL_B_CYAN, COL_RESET);

    act.sa_handler = handle_ctrl_c;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, &old_act);

    if (net_bind_server_sockets(&sockets, a) < 0)
        goto bind_sockets_failed;

    log_info("Server started\n");
    while (ctrl_c_pressed == 0)
    {
    }
    log_info("Stopping server\n");

    net_close_sockets(&sockets);
    sigaction(SIGINT, &old_act, NULL);

    return;

    bind_sockets_failed : sigaction(SIGINT, &old_act, NULL);
}

/* ------------------------------------------------------------------------- */
static void
run_client(const struct args* a)
{
    struct sigaction act;

    log_set_prefix("Client: ");
    log_set_colors(COL_B_GREEN, COL_RESET);

    act.sa_handler = handle_ctrl_c;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);

    log_info("Client started\n");
    while (ctrl_c_pressed == 0)
    {
    }
    log_info("Stopping client\n");
}

/* ------------------------------------------------------------------------- */
static uint64_t
start_background_server(const struct args* a)
{
#if defined(_WIN32)
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
stop_background_server(uint64_t pid)
{
#if defined(_WIN32)
#else
    int status;
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
    struct args args;
    switch (args_parse(&args, argc, argv))
    {
        case 0: break;
        case 1: return 0;
        default: return -1;
    }

#if defined(CLITHER_LOGGING)
    if (*args.log_file)
        log_file_open(args.log_file);
#endif

    switch (args.mode)
    {
        case MODE_HEADLESS:
            run_server(&args);
            break;
        case MODE_CLIENT:
            run_client(&args);
            break;
        case MODE_CLIENT_AND_SERVER: {
            uint64_t subprocess = start_background_server(&args);
            if (subprocess == (uint64_t)-1 || subprocess == 0)
                break;

            args.ip = "127.0.0.1";
            run_client(&args);
            stop_background_server(subprocess);
        } break;
    }

#if defined(CLITHER_LOGGING)
    log_file_close();
#endif
}
