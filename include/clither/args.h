#pragma once

#include "clither/config.h"

#include <stdint.h>

C_BEGIN

enum mode
{
#if defined(CLITHER_TESTS)
    MODE_TESTS,
#endif
#if defined(CLITHER_BENCHMARKS)
    MODE_BENCHMARKS,
#endif
    MODE_HEADLESS,
#if defined(CLITHER_GFX)
    MODE_CLIENT,
    MODE_CLIENT_AND_SERVER
#endif
};

struct args
{
#if defined(CLITHER_LOGGING)
    const char* log_file;
#endif
    const char* config_file;
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

char*
args_to_string(const char* prog_name, const struct args* a);

void
args_free_string(char* s);

C_END
