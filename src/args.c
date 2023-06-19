#include "clither/args.h"
#include "clither/cli_colors.h"
#include "clither/log.h"

#if defined(CLITHER_GFX)
#   include "clither/gfx.h"
#endif

#include <stdlib.h>
#include <stdio.h>
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

#define DEFAULT_LOG_FILE    "clither.txt"
#define DEFAULT_CONFIG_FILE "config.ini"

/* ------------------------------------------------------------------------- */
/*!
 * \brief Prints all help text and examples.
 * \param[in] prog_name Pass argv[0] here.
 */
static void
print_help(const char* prog_name)
{
    /*
     * Examples section
     */
    fprintf(stderr, SECTION "Usage:\n" RESET "  %s [" ARG2 "options" RESET "]\n\n", prog_name);
    fprintf(stderr,
        SECTION "Examples:\n" RESET
        TEXT "  Start a dedicated server (headless mode):\n" RESET
        "    %s " ARG1 "--server\n" RESET
        "    %s " ARG1 "--server --ip" RESET " 0.0.0.0" ARG1 " --port" RESET " 5678" COMMENT "    # change bind address\n" RESET
        "\n",
        prog_name, prog_name
    );
#if defined(CLITHER_GFX)
    fprintf(stderr,
        TEXT "  Host a server, then start the client  and join the server. The server will\n"
        "  stop when the client stops, since it is a child process.\n" RESET
        "    %s" ARG1 " --host\n" RESET
        "    %s" ARG1 " --host --ip" RESET " 0.0.0.0" ARG1 " --port" RESET " 5678" COMMENT "      # change bind port\n" RESET
        "\n",
        prog_name, prog_name
    );
    fprintf(stderr,
        TEXT "  Join a server\n" RESET
        "    %s" ARG1 " --ip" RESET " 192.168.1.2\n"
        "    %s" ARG1 " --ip" RESET " 192.168.1.2" ARG1 " --port" RESET " 4200" COMMENT "         # if port is custom\n" RESET
        "\n",
        prog_name, prog_name
    );
#endif

    /*
     * Available options section
     */
    fprintf(stderr,
        SECTION "Available options:\n" RESET
        "     " ARG1 " --help  " RESET "        Print this help text.\n");

    /* Unit tests */
#if defined(CLITHER_TESTS)
    fprintf(stderr, "     " ARG1 " --tests " RESET "        Run unit tests.\n");
#endif

    /* Benchmarks */
#if defined(CLITHER_BENCHMARKS)
    fprintf(stderr, "     " ARG1 " --benchmarks" RESET "    Run benchmarks.\n");
#endif

    /* Server/client options */
    fprintf(stderr, "  " ARG2 "-s" RESET "," ARG1 " --server" RESET "        Run  in  headless  mode.  This only  starts the server\n");
#if defined(CLITHER_GFX)
    fprintf(stderr,
        "  " ARG2 "-h" RESET "," ARG1 " --host  " RESET "        Spawn both the server and client, and join the server.\n"
        "                      The server will stop when the client is closed because\n"
        "                      the server is a child process of the client.\n");
#endif
#if defined(CLITHER_GFX)
    fprintf(stderr,
        "     " ARG1 " --gfx" RESET " <" ARG2 "index" RESET ">   Use graphics backend. Currently available:\n");
    {
        int i;
        for (i = 0; gfx_backends[i]; ++i)
            fprintf(stderr, "                          " ARG2 "%d" RESET ": %s\n", i, gfx_backends[i]->name);
    }
    fprintf(stderr,
        "     " ARG1 " --ip " RESET "<" ARG2 "address" RESET ">  Server  address  to  connect to. Can be a URL or an IP\n"
        "                      address. If --host or --server is used, then this sets\n"
        "                      the bind address  rather than the  address to  connect\n"
        "                      to. The client  will always use localhost or 127.0.0.1\n"
        "                      in this case.\n");
    fprintf(stderr,
        "  " ARG2 "-p" RESET "," ARG1 " --port " RESET "<" ARG2 "port" RESET ">   Port  number  of  server to  connect to.  If --host or\n"
        "                      --server is used, then this sets the  bind port rather\n"
        "                      than the port to connect to.\n");
#else
    fprintf(stderr,
        "  "      "  "       " " ARG1 " --ip " RESET "<" ARG2 "address" RESET ">  Address to bind server to.\n"
        "  " ARG2 "-p" RESET "," ARG1 " --port " RESET "<" ARG2 "port" RESET ">   Port to bind server to.\n");
#endif

    fprintf(stderr,
        "     " ARG1 " --mcd " RESET "<" ARG2 "latency" RESET "> <" ARG2 "loss" RESET "> <" ARG2 "dup" RESET "> <" ARG2 "reorder" RESET ">\n"
        "                      Enable McDonald's WiFi mode.  Latency is in ms.  Loss,\n"
        "                      dup and reorder are in percent, and specify the chance\n"
        "                      any one packet will be lost, duplicated, or reordered.\n"
        "                      If you specify values greater than 100 then the chance\n"
        "                      is  applied more than once per packet.\n");

    /* Logging options */
#if defined(CLITHER_LOGGING)
    fprintf(stderr,
        "  " ARG2 "-l" RESET "," ARG1 " --log " RESET "<" ARG2 "file" RESET ">    Write log output to a custom file. The default file is\n"
        "                      \"clither.txt\".  To disable logging to a file, set this\n"
        "                      to an empty string.\n");
#endif

    /* Disabled options */
#if !defined(CLITHER_TESTS) || !defined(CLITHER_BENCHMARKS) || !defined(CLITHER_GFX) || !defined(CLITHER_LOGGING)
    fprintf(stderr,
        SECTION "\nDisabled options:\n" RESET);
#endif
#if !defined(CLITHER_TESTS)
    fprintf(stderr, "     " RED  " --tests " RESET "        (Recompile with -DCLITHER_TESTS=ON).\n");
#endif
#if !defined(CLITHER_BENCHMARKS)
    fprintf(stderr, "     " RED  " --benchmarks" RESET "    (Recompile with -DCLITHER_BENCHMARKS=ON).\n");
#endif
#if !defined(CLITHER_GFX)
    fprintf(stderr, "  " RED "-h" RESET "," RED " --host  " RESET "        (Recompile with -DCLITHER_GFX=...)\n");
#endif
#if !defined(CLITHER_LOGGING)
    fprintf(stderr, "  " RED "-l" RESET "," RED " --log " RESET "<" RED "file" RESET ">    (Recompile with -DCLITHER_LOGGING=ON).\n");
#endif
}

/* ------------------------------------------------------------------------- */
int
args_parse(struct args* a, int argc, char* argv[])
{
    int i;
    char tests_flag = 0;
    char bench_flag = 0;
#if defined(CLITHER_GFX)
    char server_flag = 0;
    char host_flag = 0;
#endif

    /* Set defaults */
#if defined(CLITHER_LOGGING)
    a->log_file = DEFAULT_LOG_FILE;
#endif
    a->config_file = DEFAULT_CONFIG_FILE;
#if defined(CLITHER_GFX)
    a->mode = MODE_CLIENT;
#else
    a->mode = MODE_HEADLESS;
#endif

    a->ip = "";
    a->port = "";

    a->gfx_backend = 0;

    a->mcd_port = "5554";
    a->mcd_latency = 0;
    a->mcd_dup = 0;
    a->mcd_loss = 0;
    a->mcd_reorder = 0;

    for (i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '-')
        {
            if (argv[i][1] == '-')
            {
                const char* arg = &argv[i][2];
                if (strcmp(arg, "help") == 0)
                {
                    print_help(argv[0]);
                    return 1;
                }
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
                        log_err("Missing argument for --gfx\n");
                        return -1;
                    }
                    a->gfx_backend = atoi(argv[i]);

                    for (count = 0; gfx_backends[count]; ++count) {}
                    if (a->gfx_backend >= count || a->gfx_backend < 0)
                    {
                        log_err("Graphics backend index \"%d\" is out of range!\n", a->gfx_backend);
                        return -1;
                    }
                }
                else if (strcmp(arg, "server") == 0)
                    server_flag = 1;
                else if (strcmp(arg, "host") == 0)
                    host_flag = 1;
#endif
                else if (strcmp(arg, "ip") == 0)
                {
                    ++i;
                    if (i >= argc || !*argv[i])
                    {
                        log_err("Missing argument for --ip\n");
                        return -1;
                    }
                    a->ip = argv[i];
                }
                else if (strcmp(arg, "port") == 0)
                {
                    ++i;
                    if (i >= argc || !*argv[i])
                    {
                        log_err("Missing argument for --port\n");
                        return -1;
                    }
                    a->port = argv[i];
                }
                else if (strcmp(arg, "mcd") == 0)
                {
                    if (i + 4 >= argc)
                    {
                        log_err("Missing argument for --mcd\n");
                        return -1;
                    }
                    a->mcd_latency = atoi(argv[i+1]);
                    a->mcd_loss = atoi(argv[i+2]);
                    a->mcd_dup = atoi(argv[i+3]);
                    a->mcd_reorder = atoi(argv[i+4]);
                    i += 4;
                }
#if defined(CLITHER_LOGGING)
                else if (strcmp(arg, "log") == 0)
                {
                    ++i;
                    if (i >= argc /*|| !*argv[i] empty string is valid*/)
                    {
                        log_err("Missing argument for --log\n");
                        return -1;
                    }
                    a->log_file = argv[i];
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
                    if (*p == 'p')
                    {
                        ++i;
                        if (p[1] || i >= argc || !*argv[i])
                        {
                            log_err("Missing argument for -p\n");
                            return -1;
                        }
                        a->port = argv[i];
                    }
#if defined(CLITHER_GFX)
                    else if (*p == 's')
                        server_flag = 1;
                    else if (*p == 'h')
                        host_flag = 1;
#endif
#if defined(CLITHER_LOGGING)
                    else if (*p == 'l')
                    {
                        ++i;
                        if (p[1] || i >= argc /*|| !*argv[i] empty string is valid*/)
                        {
                            log_err("Missing argument for -l\n");
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

    if (0) {}
#if defined(CLITHER_TESTS)
    else if (tests_flag)
        a->mode = MODE_TESTS;
#endif
#if defined(CLITHER_BENCHMARKS)
    else if (bench_flag)
        a->mode = MODE_BENCHMARKS;
#endif
#if defined(CLITHER_GFX)
    else if (server_flag && host_flag)
    {
        log_err("Can't use \"--server\" and \"--host\" at the same time\n");
        return -1;
    }
    else if (server_flag)
        a->mode = MODE_HEADLESS;
    else if (host_flag)
        a->mode = MODE_CLIENT_AND_SERVER;
#endif

    return 0;
}
