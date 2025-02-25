#include "clither/log.h"
#include "clither/mfile.h"
#include "clither/net.h"
#include "clither/server_settings.h"
#include "clither/utf8.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum token
{
    TOK_ERROR = -1,
    TOK_END = 0,
    TOK_LBRACKET = '[',
    TOK_RBRACKET = ']',
    TOK_EQUALS = '=',
    TOK_INTEGER = 256,
    TOK_FLOAT,
    TOK_STRING,
    TOK_KEY
};

struct parser
{
    const char* filename;
    const char* source;
    int         head, tail, end;
    union
    {
        struct strview string;
        float          float_literal;
        int            integer_literal;
    } value;
};

/* ------------------------------------------------------------------------- */
static void
parser_init(struct parser* p, const struct mfile* mf, const char* filename)
{
    p->filename = filename;
    p->source = mf->address;
    p->head = 0;
    p->tail = 0;
    p->end = mf->size;
}

/* ------------------------------------------------------------------------- */
static int parser_error(struct parser* p, const char* fmt, ...)
{
    va_list        ap;
    struct strspan loc = strspan(p->tail, p->head - p->tail);

    va_start(ap, fmt);
    log_vflc(p->filename, p->source, loc, fmt, ap);
    va_end(ap);
    log_excerpt(p->source, loc);

    return -1;
}

/* ------------------------------------------------------------------------- */
static enum token scan_next_token(struct parser* p)
{
    p->tail = p->head;
    while (p->head != p->end)
    {
        /* Skip comments */
        if (p->source[p->head] == '#' || p->source[p->head] == ';')
        {
            for (p->head++; p->head != p->end; p->head++)
                if (p->source[p->head] == '\n')
                {
                    p->head++;
                    break;
                }
            p->tail = p->head;
            continue;
        }

        /* Special characters */
        if (p->source[p->head] == '[')
            return p->source[p->head++];
        if (p->source[p->head] == ']')
            return p->source[p->head++];
        if (p->source[p->head] == '=')
            return p->source[p->head++];

        /* Number */
        if (isdigit(p->source[p->head]) || p->source[p->head] == '-')
        {
            char is_float = 0;
            while (
                p->head != p->end
                && (isdigit(p->source[p->head]) || p->source[p->head] == '.'))
            {
                if (p->source[p->head] == '.')
                    is_float = 1;
                p->head++;
            }

            if (is_float)
                p->value.float_literal = strview_to_float(
                    strview(p->source, p->tail, p->head - p->tail));
            else
                p->value.integer_literal = strview_to_integer(
                    strview(p->source, p->tail, p->head - p->tail));

            return is_float ? TOK_FLOAT : TOK_INTEGER;
        }

        /* Key */
        if (isalpha(p->source[p->head]))
        {
            while (p->head != p->end && isalpha(p->source[p->head]))
                p->head++;
            p->value.string.off = p->tail;
            p->value.string.len = p->head - p->value.string.off;
            return TOK_KEY;
        }

        /* Ignore everything else */
        p->tail = ++p->head;
    }

    return TOK_END;
}

/* ------------------------------------------------------------------------- */
static void set_defaults(struct server_settings* s)
{
    s->max_players = 600;
    s->max_username_len = 32;
    s->sim_tick_rate = 60;
    s->net_tick_rate = 20;
    s->client_timeout = 5;
    s->malicious_timeout = 60;
    strcpy(s->port, NET_DEFAULT_PORT);
}

/* ------------------------------------------------------------------------- */
static int
parse_server_max_players(struct parser* p, struct server_settings* server)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 1 || p->value.integer_literal > 0xFFFF)
        return parser_error(p, "'max_players' must be 1-65535\n");

    server->max_players = (uint16_t)p->value.integer_literal;
    return 0;
}

static int
parse_server_max_username_len(struct parser* p, struct server_settings* server)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 1 || p->value.integer_literal > 255)
        return parser_error(p, "'max_username_len' must be 1-255\n");

    server->max_username_len = (uint8_t)p->value.integer_literal;
    return 0;
}

static int
parse_server_sim_tick_rate(struct parser* p, struct server_settings* server)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 1 || p->value.integer_literal > 255)
        return parser_error(p, "'sim_tick_rate' must be 1-255\n");

    server->sim_tick_rate = (uint8_t)p->value.integer_literal;
    return 0;
}

static int
parse_server_net_tick_rate(struct parser* p, struct server_settings* server)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 1 || p->value.integer_literal > 255)
        return parser_error(p, "'net_tick_rate' must be 1-255\n");

    server->net_tick_rate = (uint8_t)p->value.integer_literal;
    return 0;
}

static int
parse_server_client_timeout(struct parser* p, struct server_settings* server)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 0xFFFF)
        return parser_error(p, "'client_timeout' must be 0-65535\n");

    server->client_timeout = (uint16_t)p->value.integer_literal;
    return 0;
}

static int
parse_server_malicious_timeout(struct parser* p, struct server_settings* server)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 0xFFFF)
        return parser_error(p, "'malicious_timeout' must be 0-65535\n");

    server->malicious_timeout = (uint16_t)p->value.integer_literal;
    return 0;
}

static int parse_server_port(struct parser* p, struct server_settings* server)
{
    struct strview value;
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 65535)
        return parser_error(p, "'port' must be 0-65535\n");

    value = strview(p->source, p->tail, p->head - p->tail);
    memcpy(server->port, value.data + value.off, value.len);
    server->port[value.len] = '\0';
    return 0;
}

/* ------------------------------------------------------------------------- */
static int
parse_server_key_values(struct parser* p, struct server_settings* server)
{
    enum token     tok;
    struct strview key;

    while (1)
    {
        switch ((tok = scan_next_token(p)))
        {
            case TOK_ERROR: return -1;
            case TOK_END: return 0;

            case TOK_KEY: {
                key = p->value.string;
#define HANDLE_KEY(kname)                                                      \
    if (strview_eq_cstr(key, #kname))                                          \
    {                                                                          \
        if (scan_next_token(p) != '=')                                         \
            return parser_error(p, "Expected '=' after key\n");                \
        if (parse_server_##kname(p, server) != 0)                              \
            return -1;                                                         \
    }
                HANDLE_KEY(max_players)
                HANDLE_KEY(max_username_len)
                HANDLE_KEY(sim_tick_rate)
                HANDLE_KEY(net_tick_rate)
                HANDLE_KEY(client_timeout)
                HANDLE_KEY(malicious_timeout)
                HANDLE_KEY(port)
#undef HANDLE_KEY
                else return parser_error(
                    p, "Unknown key '%.*s'\n", key.len, key.off);
                break;
            }

            default: return tok;
        }
    }
}

/* ------------------------------------------------------------------------- */
static int parse_ini(struct parser* p, struct server_settings* server)
{
    if (scan_next_token(p) != '[')
        return parser_error(
            p, "Expected to find a section. Example: [server]\n");
    if (scan_next_token(p) != TOK_KEY)
        return parser_error(
            p,
            "Expected a section name within the brackets. Example: [server]\n");
    if (strview_eq_cstr(p->value.string, "server") == 0)
        return parser_error(
            p,
            "Unknown section '%.*s'. Expected [server]\n",
            p->value.string.len,
            p->source + p->value.string.off);
    if (scan_next_token(p) != ']')
        return parser_error(p, "Missing closing bracket ']'\n");

    return parse_server_key_values(p, server);
}

/* ------------------------------------------------------------------------- */
int server_settings_load_or_set_defaults(
    struct server_settings* server, const char* filepath)
{
    struct mfile  mf;
    struct parser p;

    set_defaults(server);

    if (!*filepath)
        return 0;

    if (mfile_map_read(&mf, filepath, 0) != 0)
    {
        log_warn(
            "Failed to open config file '%s': %s\n", filepath, strerror(errno));
        log_warn("Using default server settings\n");
        return 0;
    }

    log_dbg("Reading config from file \"%s\"\n", filepath);
    parser_init(&p, &mf, filepath);
    if (parse_ini(&p, server) != 0)
        goto parser_error;

    mfile_unmap(&mf);
    return 0;

parser_error:
    mfile_unmap(&mf);
    return -1;
}

/* ------------------------------------------------------------------------- */
void server_settings_save(const struct server_settings* s, const char* filename)
{
    FILE* fp;

    if (!*filename)
        return;

    fp = utf8_fopen_wb(filename, (int)strlen(filename));
    if (fp == NULL)
    {
        log_err(
            "Failed to save config file \"%s\": %s\n",
            filename,
            strerror(errno));
        return;
    }

    log_dbg("Saving config to \"%s\"\n", filename);
    /* clang-format off */
    fprintf(fp, "[server]\n");
    fprintf(fp, "max_players = %d     ; Total number of clients allowed to join the server\n", s->max_players);
    fprintf(fp, "max_username_len = %d ; Limits the user name length\n", s->max_username_len);
    fprintf(fp, "sim_tick_rate = %d    ; Simulation speed in Hz\n", s->sim_tick_rate);
    fprintf(fp, "net_tick_rate = %d    ; Network update speed in Hz. Should be smaller or equal to simulation speed\n", s->net_tick_rate);
    fprintf(fp, "client_timeout = %d    ; How many seconds to wait for a client before disconnecting them\n", s->client_timeout);
    fprintf(fp, "malicious_timeout = %d ; How many seconds to keep a client on the malicious list\n", s->malicious_timeout);
    fprintf(fp, "port = \"%s\"         ; Port to bind server to\n", s->port);
    /* clang-format on */
    fclose(fp);
}
