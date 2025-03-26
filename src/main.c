#include "clither/args.h"
#include "clither/benchmarks.h"
#include "clither/client.h"
#include "clither/log.h"
#include "clither/mcd_wifi.h"
#include "clither/net.h"
#include "clither/server.h"
#include "clither/settings.h"
#include "clither/signals.h"
#include "clither/tests.h"
#include "clither/thread.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct args     args;
static struct settings settings;

/* ------------------------------------------------------------------------- */
int main(int argc, char* argv[])
{
    int retval;
#if defined(CLITHER_MCD)
    struct thread* mcd_thread;
#endif

    mem_init_threadlocal();

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

    log_info("Reading settings from file \"%s\"\n", args.settings_file);
    if (settings_load(&settings, args.settings_file) != 0)
        return -1;
    if (settings_apply_args(&settings, &args) != 0)
        return -1;

    /* Install signal handlers for CTRL+C and (on windows) console close events
     */
    signals_install();

    /* Open log file */
#if defined(CLITHER_LOGGING)
    if (args.log_file)
        log_file_open(args.log_file);
    if (args.netlog_file)
        log_net_open(args.netlog_file);
#endif

    /* Init networking */
    if (net_init() < 0)
        goto net_init_failed;

    /* If McDonald's WiFi is enabled, start that */
    retval = 0;
#if defined(CLITHER_MCD)
    if (settings.mcd.enable)
    {
        mcd_thread = thread_start(run_mcd_wifi, &settings.mcd);
        if (mcd_thread == NULL)
        {
            retval = -1;
            goto start_mcd_failed;
        }
    }
#endif

    switch (args.mode)
    {
        case MODE_NONE: retval = -1; break;
#if defined(CLITHER_TESTS)
        case MODE_TESTS: {
            retval = tests_run(argc, argv);
            break;
        }
#endif
#if defined(CLITHER_BENCHMARKS)
        case MODE_BENCHMARKS: {
            retval = benchmarks_run(argc, argv);
            break;
        }
#endif
#if defined(CLITHER_SERVER)
        case MODE_SERVER: {
            struct thread* server_thread;

            log_dbg("Starting server in background thread\n");
            server_thread = thread_start(server_run, &settings.server);
            if (server_thread == NULL)
            {
                retval = -1;
                break;
            }

            retval = (intptr_t)thread_join(server_thread);
            log_dbg("Joined background server thread\n");
            break;
        }
#endif
#if defined(CLITHER_CLIENT)
        case MODE_CLIENT: {
            /* NOTE: client_run() is the only function that expects to be run
             * in the main thread. It does not call any threadlocal init
             * functions. */
            retval = (int)(intptr_t)client_run(
                &settings.client, &settings.gfx, &settings.world);
            break;
        }
#endif
#if defined(CLITHER_CLIENT) && defined(CLITHER_SERVER)
        case MODE_HOST: {
            struct thread* server_thread;

            log_dbg("Starting server in background thread\n");
            server_thread = thread_start(server_run, &settings.server);
            if (server_thread == NULL)
            {
                retval = -1;
                break;
            }

            /* The server should be running, so try to join as a client */
            retval += (int)(intptr_t)client_run(
                &settings.client, &settings.gfx, &settings.world);

            if (!signals_exit_requested())
            {
                log_note("The server will continue to run.\n");
                log_note("You can stop it by pressing CTRL+C\n");
            }

            retval += (intptr_t)thread_join(server_thread);
            log_dbg("Joined background server thread\n");
            break;
        }
#endif
    }

        /* Stop McDonald's WiFi if necessary */
#if defined(CLITHER_MCD)
    if (settings.mcd.enable)
    {
        thread_join(mcd_thread);
        log_dbg("Joined McDonald's WiFi thread\n");
    }
start_mcd_failed:
#endif
    net_deinit();
#if defined(CLITHER_LOGGING)
    log_net_close();
    log_file_close();
#endif
    signals_remove();
    (void)mem_deinit_threadlocal();

    return retval;

net_init_failed:
#if defined(CLITHER_LOGGING)
    log_file_close();
#endif
    signals_remove();
    (void)mem_deinit_threadlocal();
    return -1;
}
