#include "clither/args.h"
#include "clither/cli_colors.h"
#include "clither/log.h"

#include <stdio.h>
#include <string.h>

#define SECTION        COL_B_WHITE
#define TEXT           COL_B_WHITE
#define ARG1           COL_B_GREEN
#define ARG2           COL_B_YELLOW
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
static void
print_help(const char* prog_name)
{
    fprintf(stderr, SECTION "Usage:\n" RESET "  %s [" ARG2 "options" RESET "]\n\n", prog_name);
    fprintf(stderr,
        SECTION "Examples:\n" RESET
        TEXT "  Start a dedicated server (headless mode):\n" RESET
        "    %s " ARG1 "--server\n" RESET
        "    %s " ARG1 "--server --ip" RESET " 0.0.0.0" ARG1 " --port" RESET " 5678" COMMENT "    # change bind address\n" RESET
        "\n",
        prog_name, prog_name
    );

#if defined(CLITHER_RENDERER)
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

    fprintf(stderr,
        SECTION "Available options:\n" RESET
        "  "      "  "       " " ARG1 " --help  " RESET "        Print this help text.\n"
#if defined(CLITHER_RENDERER)
        "  " ARG2 "-s" RESET "," ARG1 " --server" RESET "        Run  in  headless  mode.  This only  starts the server\n"
        "  " ARG2 "-h" RESET "," ARG1 " --host  " RESET "        Spawn both the server and client, and join the server.\n"
        "                      The server will stop when the client is closed because\n"
        "                      the server is a child process of the client.\n"
#endif
#if defined(CLITHER_RENDERER)
        "  "      "  "       " " ARG1 " --ip " RESET "<" ARG2 "address" RESET ">  Server  address  to  connect to. Can be a URL or an IP\n"
        "                      address. If --host or --server is used, then this sets\n"
        "                      the bind address  rather than the  address to  connect\n"
        "                      to. The client  will always use localhost or 127.0.0.1\n"
        "                      in this case.\n"
        "  " ARG2 "-p" RESET "," ARG1 " --port " RESET "<" ARG2 "port" RESET ">   Port  number  of  server to  connect to.  If --host or\n"
        "                      --server is used, then this sets the  bind port rather\n"
        "                      than the port to connect to.\n"
#else
        "  "      "  "       " " ARG1 " --ip " RESET "<" ARG2 "address" RESET ">  Address to bind server to.\n"
        "  " ARG2 "-p" RESET "," ARG1 " --port " RESET "<" ARG2 "port" RESET ">   Port to bind server to.\n"
#endif
#if defined(CLITHER_LOGGING)
        "  " ARG2 "-l" RESET "," ARG1 " --log " RESET "<" ARG2 "file" RESET ">    Write log output to a custom file. The default file is\n"
        "                      \"clither.txt\".  To disable logging to a file, set this\n"
        "                      to an empty string.\n"
#endif
    );
}

/* ------------------------------------------------------------------------- */
int
args_parse(struct args* a, int argc, char* argv[])
{
    int i;
    char server_flag = 0;
    char host_flag = 0;

    /* Set defaults */
#if defined(CLITHER_LOGGING)
    a->log_file = "clither.txt";
#endif

#if defined(CLITHER_RENDERER)
    a->mode = MODE_CLIENT;
#else
    a->mode = MODE_HEADLESS;
#endif

    a->ip = "";
    a->port = "5555";

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
                else if (strcmp(arg, "server") == 0)
                    server_flag = 1;
                else if (strcmp(arg, "host") == 0)
                    host_flag = 1;
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
                else if (strcmp(argv[i], "--") == 0)
                    break;
                else
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
                    if (*p == 's')
                        server_flag = 1;
                    else if (*p == 'h')
                        host_flag = 1;
                    else if (*p == 'p')
                    {
                        ++i;
                        if (p[1] || i >= argc || !*argv[i])
                        {
                            log_err("Missing argument for -p\n");
                            return -1;
                        }
                        a->port = argv[i];
                    }
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

    if (server_flag && host_flag)
    {
        log_err("Can't use \"--server\" and \"--host\" at the same time\n");
        return -1;
    }
    else if (server_flag)
        a->mode = MODE_HEADLESS;
    else if (host_flag)
        a->mode = MODE_CLIENT_AND_SERVER;

    return 0;
}