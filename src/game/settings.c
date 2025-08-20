#include "clither/game/args.h"
#include "clither/game/settings.h"
#include "clither/platform/mfile.h"
#include "clither/platform/utf8.h"
#include "clither/util/log.h"
#include "clither/util/strview.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
int settings_apply_args(struct settings* s, const struct args* a)
{
    int len;
#define SAFE_COPY(dst, src)                                                    \
    do                                                                         \
    {                                                                          \
        len = (int)strlen(a->src);                                             \
        if (len >= (int)sizeof(dst))                                           \
            return log_err("Argument to parameter --" #src " is too long\n");  \
        strcpy(dst, a->src);                                                   \
    } while (0)

#if defined(CLITHER_CLIENT)
    if (a->username)
        SAFE_COPY(s->client.username, username);
    if (a->mode == MODE_CLIENT)
    {
        if (a->addr)
            SAFE_COPY(s->client.connect_addr, addr);
        if (a->port)
            SAFE_COPY(s->client.connect_port, port);
    }
#endif
#if defined(CLITHER_SERVER)
    if (a->mode == MODE_SERVER)
    {
        if (a->addr)
            SAFE_COPY(s->server.bind_addr, addr);
        if (a->port)
            SAFE_COPY(s->server.bind_port, port);
    }
#endif
#if defined(CLITHER_LOG)
    if (a->prefix)
        SAFE_COPY(s->client.log_prefix, prefix);
#endif
#if defined(CLITHER_MCD)
    if (a->mcd_port)
        SAFE_COPY(s->mcd.bind_port, mcd_port);
    if (a->mcd_latency > -1)
    {
        s->mcd.latency_ms = a->mcd_latency;
        s->mcd.loss_percent = a->mcd_loss;
        s->mcd.dup_percent = a->mcd_dup;
        s->mcd.reorder_percent = a->mcd_reorder;
        s->mcd.enable = 1;

        /* Route client to McDonald's WiFi */
        strcpy(s->client.connect_addr, "localhost");
        strcpy(s->client.connect_port, s->mcd.bind_port);
    }
#endif
#if defined(CLITHER_GFX)
    if (a->gfx_backend > -1)
    {
        s->gfx.backend = a->gfx_backend;
        s->gfx.enable = 1;
    }
#endif

    return 0;
}

/* ------------------------------------------------------------------------- */
void settings_init(struct settings* s)
{
#define X(sec) settings_##sec##_init(&s->sec);
    SETTINGS_SECTIONS_LIST
#undef X
}

/* ------------------------------------------------------------------------- */
void settings_deinit(struct settings* s)
{
#define X(sec) settings_##sec##_deinit(&s->sec);
    SETTINGS_SECTIONS_LIST
#undef X
}

/* ------------------------------------------------------------------------- */
int settings_load(struct settings* s, const char* filepath)
{
    struct mfile mf;

    log_info("Reading settings from file \"%s\"\n", filepath);
    if (mfile_map_read(&mf, filepath, 0) != 0)
    {
        log_warn(
            "Failed to open file \"%s\". Using default settings.\n", filepath);
        return 0;
    }

#define X(sec)                                                                 \
    if (settings_##sec##_parse(&s->sec, filepath, mf.address, mf.size) != 0)   \
        goto parser_error;
    SETTINGS_SECTIONS_LIST
#undef X

    mfile_unmap(&mf);
    return 1;

parser_error:
    mfile_unmap(&mf);
    return -1;
}

/* ------------------------------------------------------------------------- */
void settings_save(const struct settings* s, const char* filename)
{
    FILE* fp;

    if (!*filename)
        return;

    fp = utf8_fopen_wb(filename, (int)strlen(filename));
    if (fp == NULL)
    {
        log_err(
            "Failed to save settings file \"%s\": %s\n",
            filename,
            strerror(errno));
        return;
    }

    log_info("Saving settings to \"%s\"\n", filename);
#define X(sec) settings_##sec##_fwrite(&s->sec, fp);
    SETTINGS_SECTIONS_LIST
#undef X
    utf8_fclose(fp);
}
