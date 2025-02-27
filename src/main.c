#include "clither/args.h"
#include "clither/client.h"
#include "clither/log.h"
#include "clither/net.h"
#include "clither/server.h"
#include "clither/signals.h"
#include "clither/tests.h"
#include "clither/thread.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(CLITHER_BENCHMARKS)
#    include "clither/benchmarks.h"
#endif

/* ------------------------------------------------------------------------- */
int main(int argc, char* argv[])
{
    struct args args;
    int         retval;

    /* Init threadlocal memory */
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

    /* Install signal handlers for CTRL+C and (on windows) console close events
     */
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

    retval = 0;
    switch (args.mode)
    {
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
        case MODE_HEADLESS: {
            struct thread* server_thread;

            log_dbg("Starting server in background thread\n");
            server_thread = thread_start(server_run, &args);
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
#if defined(CLITHER_GFX)
        case MODE_CLIENT: {
            /* NOTE: client_run() is the only function that expects to be run
             * in the main thread. It does not call any threadlocal init
             * functions. */
            retval = (int)(intptr_t)client_run(&args);
            break;
        }
#endif
#if defined(CLITHER_GFX) && defined(CLITHER_SERVER)
        case MODE_CLIENT_AND_SERVER: {
            struct thread* server_thread;
            struct args    server_args = args;

            log_dbg("Starting server in background thread\n");
            server_thread = thread_start(server_run, &server_args);
            if (server_thread == NULL)
            {
                retval = -1;
                break;
            }

            /* The server should be running, so try to join as a client */
            args.ip = "localhost";
            retval += (int)(intptr_t)client_run(&args);

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
