#pragma once

#include "clither/config.h"

enum mode
{
#if defined(CLITHER_TESTS)
    MODE_TESTS,
#endif
#if defined(CLITHER_BENCHMARKS)
    MODE_BENCHMARKS,
#endif
#if defined(CLITHER_CLIENT)
    MODE_CLIENT,
#endif
#if defined(CLITHER_CLIENT) && defined(CLITHER_SERVER)
    MODE_HOST,
#endif
#if defined(CLITHER_SERVER)
    MODE_SERVER,
#endif
    MODE_NONE
};

struct args
{
    const char* settings_file;
#if defined(CLITHER_BOT_API)
    const char* bot_script;
#endif
#if defined(CLITHER_CLIENT) || defined(CLITHER_SERVER) || defined(CLITHER_MCD)
    const char* addr;
    const char* port;
#endif
#if defined(CLITHER_CLIENT)
    const char* username;
#endif
#if defined(CLITHER_LOG)
    const char* log_file;
    const char* netlog_file;
    const char* prefix;
#endif
#if defined(CLITHER_MCD)
    const char* mcd_port;
    int         mcd_latency, mcd_loss, mcd_dup, mcd_reorder;
#endif
#if defined(CLITHER_GFX)
    const char* pack;
    int         gfx_backend;
#endif
    enum mode mode;
};

/*!
 * \brief Parses the command line arguments.
 * \param[out] a Results of the parse are stored in this structure.
 * \param[in] argc Number of arguments.
 * \param[in] argv Array of pointers to argument strings.
 * \return
 */
int args_parse(struct args* a, int argc, char* argv[]);
