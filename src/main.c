#include "clither/audio/audio.h"
#include "clither/benchmarks.h"
#include "clither/bot/bot.h"
#include "clither/client/client.h"
#include "clither/game/args.h"
#include "clither/game/mcd_wifi.h"
#include "clither/game/resource_pack.h"
#include "clither/game/settings.h"
#include "clither/gfx/gfx.h"
#include "clither/platform/asm_optimizations.h"
#include "clither/platform/fs.h"
#include "clither/platform/net.h"
#include "clither/platform/signals.h"
#include "clither/platform/thread.h"
#include "clither/server/server.h"
#include "clither/tests.h"
#include "clither/ui/main_menu.h"
#include "clither/util/log.h"
#include "clither/util/tracker.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
int main(int argc, char* argv[])
{
    struct args                   args;
    struct settings               settings;
    struct str*                   settings_file;
    const struct audio_interface* iaudio = NULL;
    struct audio*                 audio = NULL;
    const struct gfx_interface*   igfx = NULL;
    struct gfx*                   gfx = NULL;
    struct resource_pack*         pack = NULL;
    struct fs_watch*              pack_watch = NULL;
    const struct bot_interface*   ibot = NULL;
    struct bot*                   bot = NULL;
#if defined(CLITHER_MCD)
    struct thread* mcd_thread = NULL;
#endif
    int retval = -1;

    (void)iaudio, (void)audio;
    (void)ibot, (void)bot;
    (void)igfx, (void)gfx, (void)pack, (void)pack_watch;

    if (trackers_init_tls() != 0)
        goto mem_init_failed;
    log_init();
    if (asm_optimizations_init() != 0)
        goto asm_optimizations_failed;

    /* Install signal handlers for CTRL+C and (on win32) console close events */
    signals_install();

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

#if defined(CLITHER_TESTS)
    if (args.mode == MODE_TESTS)
    {
        retval = tests_run(argc, argv);
        goto tests_or_benchmarks_run;
    }
#endif
#if defined(CLITHER_BENCHMARKS)
    if (args.mode == MODE_BENCHMARKS)
    {
        retval = benchmarks_run(argc, argv);
        goto tests_or_benchmarks_run;
    }
#endif

    str_init(&settings_file);
    if (args.settings_file)
    {
        if (str_set_cstr(&settings_file, args.settings_file) != 0)
            goto read_settings_failed;
    }
    else
    {
        if (fs_appdata_dir(&settings_file) != 0)
            goto read_settings_failed;
        if (str_join_path_cstr(&settings_file, "mechasnek") != 0)
            goto read_settings_failed;
        if (str_join_path_cstr(&settings_file, "settings.ini") != 0)
            goto read_settings_failed;
    }

    log_info("Reading settings from file \"%s\"\n", str_cstr(settings_file));
    if (settings_load(&settings, str_cstr(settings_file)) != 0)
    {
        /* settings have default values, try to save it */
        str_dirname(settings_file);
        if (fs_make_path(str_cstr(settings_file)) == 0)
        {
            if (str_join_path_cstr(&settings_file, "settings.ini") != 0)
                goto read_settings_failed;
            settings_save(&settings, str_cstr(settings_file));
        }
    }
    if (settings_apply_args(&settings, &args) != 0)
        goto read_settings_failed;

#if defined(CLITHER_LOG)
    if (args.log_file)
        log_file_open(args.log_file);
#endif

#if defined(CLITHER_GFX)
    pack = resource_pack_parse(args.pack);
    if (pack == NULL)
        goto parse_resource_pack_failed;

#    if defined(CLITHER_HOT_RELOAD)
    pack_watch = resource_pack_watch_create(pack);
    if (pack_watch == NULL)
        goto watch_resource_pack_failed;
#    endif
#endif

#if defined(CLITHER_AUDIO)
    iaudio = audio_backends[0];
    if (iaudio->init() != 0)
        goto init_audio_failed;
    audio = iaudio->create();
    if (audio == NULL)
        goto create_audio_failed;
    if (iaudio->load_resource_pack(audio, &pack->audio) != 0)
        goto load_audio_resources_failed;
#endif

#if defined(CLITHER_GFX)
    if (args.gfx_backend >= 0 && gfx_backends[args.gfx_backend] != NULL)
    {
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
#endif

#if defined(CLITHER_BOT_API)
    if (args.bot_script != NULL && bot_backends[0] != NULL)
    {
        ibot = bot_backends[0];
        if (ibot->init() != 0)
            goto init_bot_failed;
        bot = ibot->create(args.bot_script, &igfx, &gfx);
        if (bot == NULL)
            goto create_bot_failed;
    }
#endif

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
        case MODE_TESTS: break;
#endif
#if defined(CLITHER_BENCHMARKS)
        case MODE_BENCHMARKS: break;
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
            struct client client;
            client_init(&client);
            if (client_connect(
                    &client,
                    &settings,
                    settings.client.connect_addr,
                    settings.client.connect_port,
                    settings.client.username) != 0)
            {
                client_deinit(&client);
                break;
            }

            /* NOTE: client_run() is the only function that expects to be run
             * in the main thread. It does not call any threadlocal init
             * functions. */
            retval = client_run(
                iaudio,
                audio,
                &client,
                &settings,
                &igfx,
                &gfx,
                &pack,
                &pack_watch,
                ibot,
                bot);
            client_deinit(&client);
            break;
        }
#endif
#if defined(CLITHER_CLIENT) && defined(CLITHER_GFX)
        case MODE_UI: {
            retval = main_menu_run(
                iaudio,
                audio,
                &igfx,
                &gfx,
                &pack,
                &pack_watch,
                ibot,
                bot,
                &settings);
            break;
        }
#endif
    }

#if defined(CLITHER_MCD)
    /* Stop McDonald's WiFi if necessary */
    if (settings.mcd.enable)
    {
        thread_sigint(mcd_thread);
        thread_join(mcd_thread);
        log_dbg("Joined McDonald's WiFi thread\n");
    }
start_mcd_failed:
#endif

    net_deinit();
net_init_failed:

#if defined(CLITHER_BOT_API)
    if (bot != NULL)
        ibot->destroy(bot);
create_bot_failed:
    if (ibot != NULL)
        ibot->deinit();
init_bot_failed:
#endif

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
#endif

#if defined(CLITHER_AUDIO)
    if (audio != NULL && pack != NULL)
        iaudio->unload_resource_pack(audio);
load_audio_resources_failed:
    if (audio != NULL)
        iaudio->destroy(audio);
create_audio_failed:
    if (iaudio != NULL)
        iaudio->deinit();
init_audio_failed:
#endif

#if defined(CLITHER_GFX)
#    if defined(CLITHER_HOT_RELOAD)
    if (pack_watch != NULL)
        fs_watch_deinit(pack_watch);
watch_resource_pack_failed:
#    endif
    if (pack != NULL)
        resource_pack_destroy(pack);
parse_resource_pack_failed:
#endif

read_settings_failed:
    str_deinit(settings_file);

#if defined(CLITHER_LOG)
    log_file_close();
#endif

#if defined(CLITHER_TESTS) || defined(CLITHER_BENCHMARKS)
tests_or_benchmarks_run:
#endif

parse_args_failed:
    signals_remove();
asm_optimizations_failed:
    trackers_deinit_tls();
mem_init_failed:

#if defined(__EMSCRIPTEN__)
    exit(retval);
#else
    return retval;
#endif
}
