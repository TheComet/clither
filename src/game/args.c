#include "clither/game/args.h"
#include "clither/util/cli_colors.h"
#include "clither/util/log.h"

#if defined(CLITHER_GFX)
#    include "clither/gfx/gfx.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTION        COL_B_WHITE
#define TEXT           COL_B_WHITE
#define ARG1           COL_B_GREEN
#define ARG2           COL_B_YELLOW
#define RED            COL_B_RED
#define COMMENT        COL_B_CYAN
#define URL            COL_B_WHITE
#define VERSION_TEXT   COL_B_WHITE
#define VERSION_NUMBER COL_B_CYAN
#define RESET          COL_RESET

/* ------------------------------------------------------------------------- */
/*!
 * \brief Prints all help text and examples.
 * \param[in] prog_name Pass argv[0] here.
 */
static int print_help(const char* prog_name)
{
    /* clang-format off */
    log_raw(SECTION "Usage:\n" RESET "  %s [" ARG2 "options" RESET "]\n\n", prog_name);

    /*
     * Available options section
     */
    log_raw(
        SECTION "Available options:\n" RESET
        "  " ARG2 "-h" RESET "," ARG1 " --help  " RESET "          Print this help text.\n");

#if defined(CLITHER_TESTS)
    log_raw("     " ARG1 " --tests " RESET "          Run unit tests.\n");
#endif

#if defined(CLITHER_BENCHMARKS)
    log_raw("     " ARG1 " --benchmarks" RESET "      Run benchmarks.\n");
#endif

#if defined(CLITHER_SERVER)
    log_raw("  " ARG2 "-s" RESET "," ARG1 " --server" RESET "          Run in  headless  mode.  This only  starts  the  server\n");
#endif

#if defined(CLITHER_CLIENT) && defined(CLITHER_SERVER)
    log_raw(
        "     " ARG1 " --host  " RESET "          Spawn both the server and client,  and join the server.\n"
        "                        If the  client is closed,  the server  will continue to\n"
        "                        run in the background.\n");
#endif

#if defined(CLITHER_CLIENT)
    log_raw(
        "  " ARG2 "-u" RESET "," ARG1 " --username" RESET " <" ARG2 "name" RESET "> Username to use when connecting to a server.\n");
#endif

#if defined(CLITHER_CLIENT) || defined(CLITHER_SERVER)
    log_raw(
        "  " ARG2 "-a" RESET "," ARG1 " --addr " RESET "<" ARG2 "address" RESET ">  Server  address to  connect to. Can  be a URL  or an IP\n"
        "                        address. If --host or --server is used,  then this sets\n"
        "                        the bind address rather than  the address to connect to\n"
        "                        The client  will always  use localhost or  127.0.0.1 in\n"
        "                        this case.\n");
    log_raw(
        "  " ARG2 "-p" RESET "," ARG1 " --port " RESET "<" ARG2 "port" RESET ">     Port  number  of  server  to  connect to.  If --host or\n"
        "                        --server  is used, then this sets the  bind port rather\n"
        "                        than the port to connect to.\n");
#elif defined(CLITHER_CLIENT)
    log_raw(
        "  "      "  "       " " ARG1 " --addr " RESET "<" ARG2 "address" RESET ">  Server address to connect to. Can be a URL or an IP address.\n"
        "  " ARG2 "-p" RESET "," ARG1 " --port " RESET "<" ARG2 "port" RESET ">     Port number of the server to connec to.\n");
#elif defined(CLITHER_SERVER)
    log_raw(
        "  "      "  "       " " ARG1 " --addr " RESET "<" ARG2 "address" RESET ">  Address to bind server to.\n"
        "  " ARG2 "-p" RESET "," ARG1 " --port " RESET "<" ARG2 "port" RESET ">     Port to bind server to.\n");
#endif

#if defined(CLITHER_MCD)
    log_raw(
        "     " ARG1 " --mcd " RESET "<" ARG2 "latency" RESET "> <" ARG2 "loss" RESET "> <" ARG2 "dup" RESET "> <" ARG2 "reorder" RESET ">\n"
        "                        Enable McDonald's  WiFi mode.  Latency is in ms.  Loss,\n"
        "                        dup  and reorder are in percent, and specify the chance\n"
        "                        any one packet will be lost, duplicated, or  reordered.\n"
        "                        If  you specify values greater than 100 then the chance\n"
        "                        is applied more than once per packet.\n");
#endif

#if defined(CLITHER_GFX)
    log_raw(
        "     " ARG1 " --gfx" RESET " <" ARG2 "index" RESET ">     Open a window for rendering the game.\n"
        "                        Currently available backends:\n");
    {
        int i;
        for (i = 0; gfx_backends[i]; ++i)
            log_raw("                            " ARG2 "%d" RESET ": %s\n", i, gfx_backends[i]->name);
    }
#endif

    /* Logging options */
#if defined(CLITHER_LOGGING)
    log_raw(
        "  " ARG2 "-l" RESET "," ARG1 " --log " RESET "<" ARG2 "file" RESET ">      Write log output to a custom file.  The default file is\n"
        "                        \"clither.txt\".  To  disable logging to a file, set this\n"
        "                        to an empty string.\n");
    log_raw(
        "     " ARG1 " --netlog " RESET "<" ARG2 "file" RESET ">   Write  networking  log  output  to  a  custom file. The\n"
        "                        default  file  is \"net.txt\".  To  disable  logging to a\n"
        "                        file, set this to an empty string.\n");
    log_raw(
        "     " ARG1 " --prefix " RESET "<" ARG2 "name" RESET ">   Sets the \"log prefix\" of the client.  This is the  text\n"
        "                        that  appears  in front of  every log message generated\n"
        "                        by the client. The default is \"Client: \"");
#endif

    /* Disabled options */
#if !defined(CLITHER_TESTS) || !defined(CLITHER_BENCHMARKS) || !defined(CLITHER_GFX) || !defined(CLITHER_LOGGING)
    log_raw(
        SECTION "\nDisabled options:\n" RESET);
#endif
#if !defined(CLITHER_TESTS)
    log_raw("     " RED  " --tests " RESET "          (Recompile with -DCLITHER_TESTS=ON)\n");
#endif
#if !defined(CLITHER_BENCHMARKS)
    log_raw("     " RED  " --benchmarks" RESET "      (Recompile with -DCLITHER_BENCHMARKS=ON)\n");
#endif
#if !defined(CLITHER_SERVER)
    log_raw("  " RED "-s" RESET "," RED " --server" RESET "          (Recompile with -DCLITHER_SERVER=ON)\n");
#endif
#if !defined(CLITHER_SERVER) || !defined(CLITHER_CLIENT)
    log_raw("     " RED " --host " RESET "           (Recompile with -DCLITHER_CLIENT=ON -DCLITHER_SERVER=ON)\n");
#endif
#if !defined(CLITHER_CLIENT)
    log_raw(
        "  " RED "-u" RESET "," RED " --username" RESET " <" RED "name" RESET "> (Recompile with -DCLITHER_CLIENT=ON)\n");
#endif
#if !defined(CLITHER_MCD)
    log_raw("     " RED " --mcd " RESET "<" RED "latency" RESET "> <" RED "loss" RESET "> <" RED "dup" RESET "> <" RED "reorder" RESET "> (Recompile with -DCLITHER_MCD=ON)\n");
#endif
#if !defined(CLITHER_GFX)
    log_raw("     " RED " --gfx " RESET "<" RED "index" RESET ">     (Recompile with -DCLITHER_GFX=ON)\n");
#endif
#if !defined(CLITHER_LOGGING)
    log_raw("  " RED "-l" RESET "," RED " --log " RESET "<" RED "file" RESET ">      (Recompile with -DCLITHER_LOGGING=ON)\n");
    log_raw("     " RED " --netlog " RESET "<" RED "file" RESET ">   (Recompile with -DCLITHER_LOGGING=ON)\n");
    log_raw("     " RED " --prefix " RESET "<" RED "name" RESET ">   (Recompile with -DCLITHER_LOGGING=ON)\n");
#endif

    /*
     * Examples section
     */
    log_raw(SECTION "\nExamples:\n" RESET);
#if defined(CLITHER_GFX)
    log_raw(
        TEXT "  Join a server\n" RESET
        "    %s" ARG1 " --username" RESET " \"Snek\"" ARG1 " --addr" RESET " 192.168.1.2\n"
        "    %s" ARG1 " --username" RESET " \"Snek\"" ARG1 " --addr" RESET " 192.168.1.2" ARG1 " --port" RESET " 4200\n" RESET
        "\n",
        prog_name, prog_name
    );
    log_raw(
        TEXT "  Create a server and join it as a client. The server will stop when the\n"
        "  client stops, since it is a child process.\n" RESET
        "    %s" ARG1 " --host\n" RESET
        "    %s" ARG1 " --host --addr" RESET " 0.0.0.0" ARG1 " --port" RESET " 5678" COMMENT "      # change bind address\n" RESET
        "\n",
        prog_name, prog_name
    );
#endif
    log_raw(
        TEXT "  Start a dedicated server (headless mode):\n" RESET
        "    %s " ARG1 "--server\n" RESET
        "    %s " ARG1 "--server --addr" RESET " 0.0.0.0" ARG1 " --port" RESET " 5678" COMMENT "    # change bind address\n" RESET
        "\n",
        prog_name, prog_name
    );
    /* clang-format on */

    return 1;
}

/* ------------------------------------------------------------------------- */
int args_parse(struct args* a, int argc, char* argv[])
{
    int  i;
    char tests_flag = 0;
    char bench_flag = 0;
#if defined(CLITHER_SERVER)
    char server_flag = 0;
#endif
#if defined(CLITHER_CLIENT) && defined(CLITHER_SERVER)
    char host_flag = 0;
#endif

    /* Set defaults */
    a->settings_file = "settings.ini";
#if defined(CLITHER_CLIENT) || defined(CLITHER_SERVER) || defined(CLITHER_MCD)
    a->addr = NULL;
    a->port = NULL;
#endif
#if defined(CLITHER_CLIENT)
    a->username = NULL;
#endif
#if defined(CLITHER_LOGGING)
    a->log_file = "clither.txt";
    a->netlog_file = "net.txt";
    a->prefix = NULL;
#endif
#if defined(CLITHER_CLIENT)
    a->mode = MODE_CLIENT;
#elif defined(CLITHER_SERVER)
    a->mode = MODE_SERVER;
#else
    a->mode = MODE_NONE;
#endif
#if defined(CLITHER_GFX)
    a->gfx_backend = -1;
#endif
#if defined(CLITHER_MCD)
    a->mcd_port = NULL;
    a->mcd_latency = -1;
    a->mcd_dup = -1;
    a->mcd_loss = -1;
    a->mcd_reorder = -1;
#endif

    if (argc == 1)
        return print_help(argv[0]);

    for (i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '-')
        {
            if (argv[i][1] == '-')
            {
                const char* arg = &argv[i][2];
                if (strcmp(arg, "help") == 0)
                    return print_help(argv[0]);
#if defined(CLITHER_TESTS)
                else if (strcmp(arg, "tests") == 0)
                    tests_flag = 1;
#endif
#if defined(CLITHER_BENCHMARKS)
                else if (strcmp(arg, "benchmarks") == 0)
                    bench_flag = 1;
#endif
#if defined(CLITHER_GFX)
                else if (strcmp(arg, "gfx") == 0)
                {
                    int count;
                    ++i;
                    if (i >= argc || !*argv[i])
                    {
                        log_err("Missing argument for --gfx <index>\n");
                        return -1;
                    }
                    a->gfx_backend = atoi(argv[i]);

                    for (count = 0; gfx_backends[count]; ++count)
                    {
                    }
                    if (a->gfx_backend >= count || a->gfx_backend < 0)
                    {
                        log_err(
                            "Graphics backend index \"%d\" is out of range!\n",
                            a->gfx_backend);
                        return -1;
                    }
                }
#endif
#if defined(CLITHER_SERVER)
                else if (strcmp(arg, "server") == 0)
                    server_flag = 1;
#endif
#if defined(CLITHER_CLIENT) && defined(CLITHER_SERVER)
                else if (strcmp(arg, "host") == 0)
                    host_flag = 1;
#endif
#if defined(CLITHER_CLIENT) || defined(CLITHER_SERVER) || defined(CLITHER_MCD)
                else if (strcmp(arg, "addr") == 0)
                {
                    ++i;
                    if (i >= argc || !*argv[i])
                    {
                        log_err("Missing argument for --addr <address>\n");
                        return -1;
                    }
                    a->addr = argv[i];
                }
                else if (strcmp(arg, "port") == 0)
                {
                    ++i;
                    if (i >= argc || !*argv[i])
                    {
                        log_err("Missing argument for --port <port>\n");
                        return -1;
                    }
                    a->port = argv[i];
                }
#endif
#if defined(CLITHER_CLIENT)
                else if (strcmp(arg, "username") == 0)
                {
                    ++i;
                    if (i >= argc || !*argv[i])
                    {
                        log_err("Missing argument for --username <name>\n");
                        return -1;
                    }
                    a->username = argv[i];
                }
#endif
#if defined(CLITHER_MCD)
                else if (strcmp(arg, "mcd") == 0)
                {
                    if (i + 4 >= argc)
                    {
                        log_err(
                            "Missing argument for --mcd <latency> <loss> <dup> "
                            "<reorder>\n");
                        return -1;
                    }
                    a->mcd_latency = atoi(argv[i + 1]);
                    a->mcd_loss = atoi(argv[i + 2]);
                    a->mcd_dup = atoi(argv[i + 3]);
                    a->mcd_reorder = atoi(argv[i + 4]);
                    i += 4;
                }
#endif
#if defined(CLITHER_LOGGING)
                else if (strcmp(arg, "log") == 0)
                {
                    ++i;
                    if (i >= argc /*|| !*argv[i] empty string is valid*/)
                    {
                        log_err("Missing argument for --log <file>\n");
                        return -1;
                    }
                    a->log_file = argv[i];
                }
                else if (strcmp(arg, "netlog") == 0)
                {
                    ++i;
                    if (i >= argc /*|| !*argv[i] empty string is valid*/)
                    {
                        log_err("Missing argument for --netlog <file>\n");
                        return -1;
                    }
                    a->netlog_file = argv[i];
                }
                else if (strcmp(arg, "prefix") == 0)
                {
                    ++i;
                    if (i >= argc || !*argv[i])
                    {
                        log_err("Missing argument for --prefix <name>\n");
                        return -1;
                    }
                    a->prefix = argv[i];
                }
#endif
                else if (strcmp(argv[i], "--") == 0)
                    break;
                else if (tests_flag == 0 && bench_flag == 0)
                {
                    log_err("Unknown option \"%s\"\n", argv[i]);
                    return -1;
                }
            }
            else
            {
                const char* p;
                for (p = &argv[i][1]; *p; ++p)
                {
                    if (0)
                    {
                    }
#if defined(CLITHER_SERVER) || defined(CLITHER_CLIENT) || defined(CLITHER_MCD)
                    else if (*p == 'p')
                    {
                        ++i;
                        if (p[1] || i >= argc || !*argv[i])
                        {
                            log_err("Missing argument for -p <port>\n");
                            return -1;
                        }
                        a->port = argv[i];
                    }
#endif
#if defined(CLITHER_SERVER)
                    else if (*p == 's')
                        server_flag = 1;
#endif
#if defined(CLITHER_CLIENT)
                    else if (*p == 'u')
                    {
                        ++i;
                        if (p[1] || i >= argc || !*argv[i])
                        {
                            log_err("Missing argument for -u <name>\n");
                            return -1;
                        }
                        a->username = argv[i];
                    }
#endif
#if defined(CLITHER_LOGGING)
                    else if (*p == 'l')
                    {
                        ++i;
                        if (p[1] ||
                            i >= argc /*|| !*argv[i] empty string is valid*/)
                        {
                            log_err("Missing argument for -l <file>\n");
                            return -1;
                        }
                        a->log_file = argv[i];
                    }
#endif
                    else
                    {
                        log_err("Unknown option \"-%c\"\n", *p);
                        return -1;
                    }
                }

                if (!argv[i][1])
                {
                    log_err("Unknown option \"%s\"\n", argv[i]);
                    return -1;
                }
            }
        }
        else
        {
            log_err("Invalid option \"%s\"\n", argv[i]);
            return -1;
        }
    }

    if (0)
    {
    }
#if defined(CLITHER_TESTS)
    else if (tests_flag)
        a->mode = MODE_TESTS;
#endif
#if defined(CLITHER_BENCHMARKS)
    else if (bench_flag)
        a->mode = MODE_BENCHMARKS;
#endif
#if defined(CLITHER_SERVER)
    else if (server_flag)
        a->mode = MODE_SERVER;
#endif
#if defined(CLITHER_CLIENT) && defined(CLITHER_SERVER)
    else if (host_flag)
        a->mode = MODE_HOST;
#endif

#if defined(CLITHER_CLIENT) && defined(CLITHER_SERVER)
    if (server_flag && host_flag)
    {
        log_err("Can't use \"--server\" and \"--host\" at the same time\n");
        return -1;
    }
#endif

    return 0;
}
