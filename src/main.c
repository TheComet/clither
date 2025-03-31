#include "clither/benchmarks.h"
#include "clither/bot/bot.h"
#include "clither/client/client.h"
#include "clither/game/args.h"
#include "clither/game/mcd_wifi.h"
#include "clither/game/resource_pack.h"
#include "clither/game/settings.h"
#include "clither/gfx/gfx.h"
#include "clither/platform/net.h"
#include "clither/platform/signals.h"
#include "clither/platform/thread.h"
#include "clither/server/server.h"
#include "clither/tests.h"
#include "clither/util/log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct args     args;
static struct settings settings;

/* ------------------------------------------------------------------------- */
int main(int argc, char* argv[])
{
#if defined(CLITHER_CLIENT)
    struct resource_pack*       pack;
    const struct gfx_interface* igfx;
    struct gfx*                 gfx;
#endif
#if defined(CLITHER_BOT_API)
    const struct bot_interface* ibot;
    struct bot*                 bot;
#endif
#if defined(CLITHER_MCD)
    struct thread* mcd_thread;
#endif
    int retval = -1;

    mem_init_threadlocal();
    log_init();

    /*
     * Parse command line args before doing anything else. This function
     * returns -1 if an error occurred, 0 if we can continue running, and 1
     * if --help appeared, in which case we should exit.
     */
    switch (args_parse(&args, argc, argv))
    {
        case 0: break;
        case 1: return 0;
        default: goto parse_args_failed;
    }

    log_info("Reading settings from file \"%s\"\n", args.settings_file);
    if (settings_load(&settings, args.settings_file) != 0)
        goto parse_args_failed;
    if (settings_apply_args(&settings, &args) != 0)
        goto parse_args_failed;

    /* Install signal handlers for CTRL+C and (on windows) console close events
     */
    signals_install();

#if defined(CLITHER_LOGGING)
    /* Open log files */
    if (args.log_file)
        log_file_open(args.log_file);
    if (args.netlog_file)
        log_net_open(args.netlog_file);
#endif

#if defined(CLITHER_BOT_API)
    /* Create bot if specified on the command line */
    ibot = NULL;
    bot = NULL;
    if (args.bot_script != NULL && bot_backends[0] != NULL)
    {
        ibot = bot_backends[0];
        if (ibot->init() != 0)
            goto init_bot_failed;
        bot = ibot->create(args.bot_script);
        if (bot == NULL)
            goto create_bot_failed;
    }
#endif

#if defined(CLITHER_CLIENT)
    pack = NULL;
    igfx = NULL;
    gfx = NULL;
#    if defined(CLITHER_GFX)
    /* Create graphics backend if specified on the command line, or if no bot
     * was specified. */
    if (args.gfx_backend >= 0 && gfx_backends[args.gfx_backend] != NULL)
    {
        pack = resource_pack_parse("packs/horror");
        if (pack == NULL)
            goto parse_resource_pack_failed;

        igfx = gfx_backends[args.gfx_backend];
        log_info("Using graphics backend: %s\n", igfx->name);
        if (igfx->init() != 0)
            goto init_gfx_failed;
        gfx = igfx->create(settings.gfx.width, settings.gfx.height);
        if (gfx == NULL)
            goto create_gfx_failed;
        if (igfx->load_resource_pack(gfx, pack) < 0)
            goto load_resource_pack_failed;
    }
#    endif
#endif

    /* Init networking */
    if (net_init() < 0)
        goto net_init_failed;

#if defined(CLITHER_MCD)
    /* If McDonald's WiFi is enabled, start that */
    if (settings.mcd.enable)
    {
        mcd_thread = thread_start(run_mcd_wifi, &settings.mcd);
        if (mcd_thread == NULL)
            goto start_mcd_failed;
    }
#endif

    switch (args.mode)
    {
        case MODE_NONE: break;
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
                break;

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
                &settings.client, &pack, &igfx, &gfx, ibot, bot);
            break;
        }
#endif
#if defined(CLITHER_CLIENT) && defined(CLITHER_SERVER)
        case MODE_HOST: {
            struct thread* server_thread;

            log_dbg("Starting server in background thread\n");
            server_thread = thread_start(server_run, &settings.server);
            if (server_thread == NULL)
                break;

            /* The server should be running, so try to join as a client */
            retval = (int)(intptr_t)client_run(
                &settings.client, &pack, &igfx, &gfx, ibot, bot);

            if (!signals_exit_requested())
            {
                log_note("The server will continue to run.\n");
                log_note("You can stop it by pressing CTRL+C\n");
            }

            if ((intptr_t)thread_join(server_thread) != 0)
                retval = -1;
            log_dbg("Joined background server thread\n");
            break;
        }
#endif
    }

#if defined(CLITHER_MCD)
    /* Stop McDonald's WiFi if necessary */
    if (settings.mcd.enable)
    {
        thread_join(mcd_thread);
        log_dbg("Joined McDonald's WiFi thread\n");
    }
start_mcd_failed:
#endif

net_init_failed:

#if defined(CLITHER_GFX)
    if (gfx != NULL && pack != NULL)
        igfx->unload_resource_pack(gfx, pack);
load_resource_pack_failed:
    if (gfx != NULL)
        igfx->destroy(gfx);
create_gfx_failed:
    if (igfx != NULL)
        igfx->deinit();
init_gfx_failed:
    if (pack != NULL)
        resource_pack_destroy(pack);
parse_resource_pack_failed:
#endif

#if defined(CLITHER_BOT_API)
    if (bot != NULL)
        ibot->destroy(bot);
create_bot_failed:
    if (ibot != NULL)
        ibot->deinit();
init_bot_failed:
#endif

#if defined(CLITHER_LOGGING)
    log_net_close();
    log_file_close();
#endif

    signals_remove();
parse_args_failed:
    (void)mem_deinit_threadlocal();
    return retval;
}
