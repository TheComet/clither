#pragma once

#include "clither/config.h"

#include <stdint.h>

enum mode
{
    MODE_HEADLESS,
#if defined(CLITHER_RENDERER)
    MODE_CLIENT,
    MODE_CLIENT_AND_SERVER
#endif
};

struct args
{
#if defined(CLITHER_LOGGING)
    const char* log_file;
#endif
    const char* ip;
    const char* port;
    enum mode mode;
};

/*!
 * \brief Parses the command line arguments.
 * \param[out] a Results of the parse are stored in this structure.
 * \param[in] argc Number of arguments.
 * \param[in] argv Array of pointers to argument strings.
 * \return
 */
int
args_parse(struct args* a, int argc, char* argv[]);
