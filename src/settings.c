#include "clither/args.h"
#include "clither/log.h"
#include "clither/mfile.h"
#include "clither/net.h"
#include "clither/settings.h"
#include "clither/strview.h"
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
CLITHER_PRINTF_FORMAT(2, 3)
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
            while (p->head != p->end &&
                   (isdigit(p->source[p->head]) || p->source[p->head] == '.'))
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

        /* String literal ".*?" (spans over newlines)*/
        if (p->source[p->head] == '"')
        {
            int tail = ++p->head;
            for (; p->head != p->end; ++p->head)
                if (p->source[p->head] == '"' && p->source[p->head - 1] != '\\')
                    break;
            if (p->head == p->end)
                return parser_error(p, "Missing closing quote on string\n");
            p->value.string = strview(p->source, tail, p->head++ - tail);
            return TOK_STRING;
        }

        /* Key */
        if (isalpha(p->source[p->head]))
        {
            while (p->head != p->end &&
                   (isalpha(p->source[p->head]) || p->source[p->head] == '_'))
            {
                p->head++;
            }
            p->value.string = strview(p->source, p->tail, p->head - p->tail);
            return TOK_KEY;
        }

        /* Ignore everything else */
        p->tail = ++p->head;
    }

    return TOK_END;
}

/* ------------------------------------------------------------------------- */
void settings_set_defaults(struct settings* s)
{
    /* [server] */
    s->server.max_players = 600;
    s->server.client_timeout = 5;
    s->server.malicious_timeout = 60;
    s->server.max_username_len = 32;
    s->server.sim_tick_rate = 60;
    s->server.net_tick_rate = 20;
    strcpy(s->server.bind_addr, "");
    strcpy(s->server.bind_port, "5555");
    strcpy(s->server.log_prefix, "Server: ");

    /* [world] */
    s->world.food_count = 10000;
    s->world.inner_radius = 120;
    s->world.ring_start = 190;
    s->world.ring_end = 255;

    /* [client] */
    strcpy(s->client.log_prefix, "Client: ");
    strcpy(s->client.username, "Snek :D");
    strcpy(s->client.connect_addr, "localhost");
    strcpy(s->client.connect_port, "5555");

    /* [gfx] */
    s->gfx.width = 1280;
    s->gfx.height = 960;
    s->gfx.backend = 0;
    s->gfx.enable = 0;

    /* [mcd] */
    strcpy(s->mcd.bind_addr, "");
    strcpy(s->mcd.bind_port, "5554");
    strcpy(s->mcd.connect_addr, "localhost");
    strcpy(s->mcd.connect_port, "5555");
}

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

#if defined(CLITHER_CLIENT) || defined(CLITHER_SERVER) || defined(CLITHER_MCD)
    if (a->addr)
        SAFE_COPY(s->server.bind_addr, addr);
    if (a->port)
        SAFE_COPY(s->server.bind_port, port);
#endif
#if defined(CLITHER_CLIENT)
    if (a->username)
        SAFE_COPY(s->client.username, username);
#endif
#if defined(CLITHER_CLIENT) && defined(CLITHER_SERVER)
    if (a->mode == MODE_HOST)
    {
        /* Route client to server */
        strcpy(s->client.connect_addr, "localhost");
        strcpy(s->client.connect_port, s->server.bind_port);
    }
#endif
#if defined(CLITHER_LOGGING)
    if (a->prefix)
        SAFE_COPY(s->client.log_prefix, netlog_file);
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
static int parse_server_max_players(struct parser* p, struct settings_server* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 1 || p->value.integer_literal > 0xFFFF)
        return parser_error(p, "\"max_players\" must be 1-65535\n");

    s->max_players = (uint16_t)p->value.integer_literal;
    return 0;
}
static int
parse_server_max_username_len(struct parser* p, struct settings_server* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 1 || p->value.integer_literal > 255)
        return parser_error(p, "\"max_username_len\" must be 1-255\n");

    s->max_username_len = (uint8_t)p->value.integer_literal;
    return 0;
}
static int
parse_server_sim_tick_rate(struct parser* p, struct settings_server* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 1 || p->value.integer_literal > 255)
        return parser_error(p, "\"sim_tick_rate\" must be 1-255\n");

    s->sim_tick_rate = (uint8_t)p->value.integer_literal;
    return 0;
}
static int
parse_server_net_tick_rate(struct parser* p, struct settings_server* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 1 || p->value.integer_literal > 255)
        return parser_error(p, "\"net_tick_rate\" must be 1-255\n");

    s->net_tick_rate = (uint8_t)p->value.integer_literal;
    return 0;
}
static int
parse_server_client_timeout(struct parser* p, struct settings_server* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 0xFFFF)
        return parser_error(p, "\"client_timeout\" must be 0-65535\n");

    s->client_timeout = (uint16_t)p->value.integer_literal;
    return 0;
}
static int
parse_server_malicious_timeout(struct parser* p, struct settings_server* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 0xFFFF)
        return parser_error(p, "\"malicious_timeout\" must be 0-65535\n");

    s->malicious_timeout = (uint16_t)p->value.integer_literal;
    return 0;
}
static int parse_server_bind_addr(struct parser* p, struct settings_server* s)
{
    struct strview value;
    if (scan_next_token(p) != TOK_STRING)
        return parser_error(p, "Expected a string value\n");
    value = p->value.string;
    if (value.len >= (int)sizeof(s->bind_addr))
        return parser_error(
            p,
            "\"bind_addr\" can't be longer than %d characters\n",
            (int)sizeof(s->bind_addr) - 1);

    memcpy(s->bind_addr, value.data + value.off, value.len);
    s->bind_addr[value.len] = '\0';
    return 0;
}
static int parse_server_bind_port(struct parser* p, struct settings_server* s)
{
    struct strview value;
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 65535)
        return parser_error(p, "\"bind_port\" must be 0-65535\n");

    value = strview(p->source, p->tail, p->head - p->tail);
    memcpy(s->bind_port, value.data + value.off, value.len);
    s->bind_port[value.len] = '\0';
    return 0;
}

static int parse_server_log_prefix(struct parser* p, struct settings_server* s)
{
    struct strview value;
    if (scan_next_token(p) != TOK_STRING)
        return parser_error(p, "Expected a string value\n");
    value = p->value.string;
    if (value.len >= (int)sizeof(s->log_prefix))
        return parser_error(
            p,
            "\"log_prefix\" can't be longer than %d characters\n",
            (int)sizeof(s->log_prefix) - 1);

    memcpy(s->log_prefix, value.data + value.off, value.len);
    s->log_prefix[value.len] = '\0';
    return 0;
}
static int parse_server_key_values(struct parser* p, struct settings_server* s)
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
                /* clang-format off */
                if (0) {}
#define HANDLE_KEY(kname)                                                      \
                else if (strview_eq_cstr(key, #kname))                         \
                {                                                              \
                    if (scan_next_token(p) != '=')                             \
                        return parser_error(p, "Expected \"=\" after key\n");  \
                    if (parse_server_##kname(p, s) != 0)                       \
                        return -1;                                             \
                }
                HANDLE_KEY(max_players)
                HANDLE_KEY(max_username_len)
                HANDLE_KEY(sim_tick_rate)
                HANDLE_KEY(net_tick_rate)
                HANDLE_KEY(client_timeout)
                HANDLE_KEY(malicious_timeout)
                HANDLE_KEY(bind_addr)
                HANDLE_KEY(bind_port)
                HANDLE_KEY(log_prefix)
#undef HANDLE_KEY
                else
                {
                    return parser_error(
                        p, "Unknown key \"%.*s\"\n", key.len, key.data + key.off);
                }
                /* clang-format on */
                break;
            }

            default: return tok;
        }
    }
}

/* ------------------------------------------------------------------------- */
static int parse_world_food_count(struct parser* p, struct settings_world* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 100000000)
        return parser_error(p, "\"food_count\" must be 0-100000000\n");

    s->food_count = (uint32_t)p->value.integer_literal;
    return 0;
}
static int parse_world_inner_radius(struct parser* p, struct settings_world* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 5 || p->value.integer_literal > 255)
        return parser_error(p, "\"inner_radius\" must be 5-255\n");

    s->inner_radius = (uint8_t)p->value.integer_literal;
    return 0;
}
static int parse_world_ring_start(struct parser* p, struct settings_world* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 5 || p->value.integer_literal > 255)
        return parser_error(p, "\"ring_start\" must be 5-255\n");

    s->ring_start = (uint8_t)p->value.integer_literal;
    return 0;
}
static int parse_world_ring_end(struct parser* p, struct settings_world* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 5 || p->value.integer_literal > 255)
        return parser_error(p, "\"ring_end\" must be 5-255\n");

    s->ring_end = (uint8_t)p->value.integer_literal;
    return 0;
}
static int parse_world_key_values(struct parser* p, struct settings_world* s)
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
                /* clang-format off */
                if (0) {}
#define HANDLE_KEY(kname)                                                      \
                else if (strview_eq_cstr(key, #kname))                         \
                {                                                              \
                    if (scan_next_token(p) != '=')                             \
                        return parser_error(p, "Expected \"=\" after key\n");  \
                    if (parse_world_##kname(p, s) != 0)                        \
                        return -1;                                             \
                }
                HANDLE_KEY(food_count)
                HANDLE_KEY(inner_radius)
                HANDLE_KEY(ring_start)
                HANDLE_KEY(ring_end)
#undef HANDLE_KEY
                else
                {
                    return parser_error(
                        p, "Unknown key \"%.*s\"\n", key.len, key.data + key.off);
                }
                /* clang-format on */
                break;
            }

            default: return tok;
        }
    }
}

/* ------------------------------------------------------------------------- */
static int parse_client_username(struct parser* p, struct settings_client* s)
{
    struct strview value;
    if (scan_next_token(p) != TOK_STRING)
        return parser_error(p, "Expected a string value\n");
    value = p->value.string;
    if (value.len >= (int)sizeof(s->username))
        return parser_error(
            p,
            "\"username\" can't be longer than %d characters\n",
            (int)sizeof(s->username) - 1);

    memcpy(s->username, value.data + value.off, value.len);
    s->username[value.len] = '\0';
    return 0;
}
static int
parse_client_connect_addr(struct parser* p, struct settings_client* s)
{
    struct strview value;
    if (scan_next_token(p) != TOK_STRING)
        return parser_error(p, "Expected a string value\n");
    value = p->value.string;
    if (value.len >= (int)sizeof(s->connect_addr))
        return parser_error(
            p,
            "\"connect_addr\" can't be longer than %d characters\n",
            (int)sizeof(s->connect_addr) - 1);

    memcpy(s->connect_addr, value.data + value.off, value.len);
    s->connect_addr[value.len] = '\0';
    return 0;
}

static int
parse_client_connect_port(struct parser* p, struct settings_client* s)
{
    struct strview value;
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 65535)
        return parser_error(p, "\"connect_port\" must be 0-65535\n");

    value = strview(p->source, p->tail, p->head - p->tail);
    memcpy(s->connect_port, value.data + value.off, value.len);
    s->connect_port[value.len] = '\0';
    return 0;
}
static int parse_client_log_prefix(struct parser* p, struct settings_client* s)
{
    struct strview value;
    if (scan_next_token(p) != TOK_STRING)
        return parser_error(p, "Expected a string value\n");
    value = p->value.string;
    if (value.len >= (int)sizeof(s->log_prefix))
        return parser_error(
            p,
            "\"log_prefix\" can't be longer than %d characters\n",
            (int)sizeof(s->log_prefix) - 1);

    memcpy(s->log_prefix, value.data + value.off, value.len);
    s->log_prefix[value.len] = '\0';
    return 0;
}
static int parse_client_key_values(struct parser* p, struct settings_client* s)
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
                /* clang-format off */
                if (0) {}
#define HANDLE_KEY(kname)                                                      \
                else if (strview_eq_cstr(key, #kname))                         \
                {                                                              \
                    if (scan_next_token(p) != '=')                             \
                        return parser_error(p, "Expected \"=\" after key\n");    \
                    if (parse_client_##kname(p, s) != 0)              \
                        return -1;                                             \
                }
                HANDLE_KEY(username)
                HANDLE_KEY(connect_addr)
                HANDLE_KEY(connect_port)
                HANDLE_KEY(log_prefix)
#undef HANDLE_KEY
                else
                {
                    return parser_error(
                        p, "Unknown key \"%.*s\"\n", key.len, key.data + key.off);
                }
                /* clang-format on */
                break;
            }

            default: return tok;
        }
    }
}

/* ------------------------------------------------------------------------- */
static int parse_gfx_width(struct parser* p, struct settings_gfx* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 64 || p->value.integer_literal > 65535)
        return parser_error(p, "\"width\" must be 64-65535\n");

    s->width = p->value.integer_literal;
    return 0;
}
static int parse_gfx_height(struct parser* p, struct settings_gfx* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 64 || p->value.integer_literal > 65535)
        return parser_error(p, "\"height\" must be 64-65535\n");

    s->height = p->value.integer_literal;
    return 0;
}
static int parse_gfx_backend(struct parser* p, struct settings_gfx* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0)
        return parser_error(p, "\"backend\" must be 64-65535\n");

    s->backend = p->value.integer_literal;
    return 0;
}
static int parse_gfx_enable(struct parser* p, struct settings_gfx* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 1)
        return parser_error(p, "\"enable\" must be 0 or 1\n");

    s->enable = p->value.integer_literal ? 1 : 0;
    return 0;
}
static int parse_gfx_fullscreen(struct parser* p, struct settings_gfx* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 1)
        return parser_error(p, "\"fullscreen\" must be 0 or 1\n");

    s->fullscreen = p->value.integer_literal ? 1 : 0;
    return 0;
}
static int parse_gfx_key_values(struct parser* p, struct settings_gfx* s)
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
                /* clang-format off */
                if (0) {}
#define HANDLE_KEY(kname)                                                      \
                else if (strview_eq_cstr(key, #kname))                         \
                {                                                              \
                    if (scan_next_token(p) != '=')                             \
                        return parser_error(p, "Expected \"=\" after key\n");    \
                    if (parse_gfx_##kname(p, s) != 0)              \
                        return -1;                                             \
                }
                HANDLE_KEY(width)
                HANDLE_KEY(height)
                HANDLE_KEY(backend)
                HANDLE_KEY(enable)
                HANDLE_KEY(fullscreen)
#undef HANDLE_KEY
                else
                {
                    return parser_error(
                        p, "Unknown key \"%.*s\"\n", key.len, key.data + key.off);
                }
                /* clang-format on */
                break;
            }

            default: return tok;
        }
    }
}

/* ------------------------------------------------------------------------- */
static int parse_mcd_latency_ms(struct parser* p, struct settings_mcd* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 4000)
        return parser_error(p, "\"latency_ms\" must be 0-4000\n");

    s->latency_ms = p->value.integer_literal;
    return 0;
}
static int parse_mcd_loss_percent(struct parser* p, struct settings_mcd* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 1000)
        return parser_error(p, "\"loss_percent\" must be 0-1000\n");

    s->loss_percent = p->value.integer_literal;
    return 0;
}
static int parse_mcd_reorder_percent(struct parser* p, struct settings_mcd* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 1000)
        return parser_error(p, "\"reorder_percent\" must be 0-1000\n");

    s->reorder_percent = p->value.integer_literal;
    return 0;
}
static int parse_mcd_dup_percent(struct parser* p, struct settings_mcd* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 1000)
        return parser_error(p, "\"dup_percent\" must be 0-1000\n");

    s->dup_percent = p->value.integer_literal;
    return 0;
}
static int parse_mcd_bind_addr(struct parser* p, struct settings_mcd* s)
{
    struct strview value;
    if (scan_next_token(p) != TOK_STRING)
        return parser_error(p, "Expected a string value\n");
    value = p->value.string;
    if (value.len >= (int)sizeof(s->bind_addr))
        return parser_error(
            p,
            "\"bind_addr\" can't be longer than %d characters\n",
            (int)sizeof(s->bind_addr) - 1);

    memcpy(s->bind_addr, value.data + value.off, value.len);
    s->bind_addr[value.len] = '\0';
    return 0;
}
static int parse_mcd_bind_port(struct parser* p, struct settings_mcd* s)
{
    struct strview value;
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 65535)
        return parser_error(p, "\"bind_port\" must be 0-65535\n");

    value = strview(p->source, p->tail, p->head - p->tail);
    memcpy(s->bind_port, value.data + value.off, value.len);
    s->bind_port[value.len] = '\0';
    return 0;
}
static int parse_mcd_connect_addr(struct parser* p, struct settings_mcd* s)
{
    struct strview value;
    if (scan_next_token(p) != TOK_STRING)
        return parser_error(p, "Expected a string value\n");
    value = p->value.string;
    if (value.len >= (int)sizeof(s->connect_addr))
        return parser_error(
            p,
            "\"connect_addr\" can't be longer than %d characters\n",
            (int)sizeof(s->connect_addr) - 1);

    memcpy(s->connect_addr, value.data + value.off, value.len);
    s->connect_addr[value.len] = '\0';
    return 0;
}
static int parse_mcd_connect_port(struct parser* p, struct settings_mcd* s)
{
    struct strview value;
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 65535)
        return parser_error(p, "\"connect_port\" must be 0-65535\n");

    value = strview(p->source, p->tail, p->head - p->tail);
    memcpy(s->connect_port, value.data + value.off, value.len);
    s->connect_port[value.len] = '\0';
    return 0;
}
static int parse_mcd_enable(struct parser* p, struct settings_mcd* s)
{
    if (scan_next_token(p) != TOK_INTEGER)
        return parser_error(p, "Expected an integer value\n");

    if (p->value.integer_literal < 0 || p->value.integer_literal > 1)
        return parser_error(p, "\"enable\" must be 0 or 1\n");

    s->enable = p->value.integer_literal ? 1 : 0;
    return 0;
}
static int parse_mcd_key_values(struct parser* p, struct settings_mcd* s)
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
                /* clang-format off */
                if (0) {}
#define HANDLE_KEY(kname)                                                      \
                else if (strview_eq_cstr(key, #kname))                         \
                {                                                              \
                    if (scan_next_token(p) != '=')                             \
                        return parser_error(p, "Expected \"=\" after key\n");    \
                    if (parse_mcd_##kname(p, s) != 0)              \
                        return -1;                                             \
                }
                HANDLE_KEY(latency_ms)
                HANDLE_KEY(loss_percent)
                HANDLE_KEY(dup_percent)
                HANDLE_KEY(reorder_percent)
                HANDLE_KEY(bind_addr)
                HANDLE_KEY(bind_port)
                HANDLE_KEY(connect_addr)
                HANDLE_KEY(connect_port)
                HANDLE_KEY(enable)
#undef HANDLE_KEY
                else
                {
                    return parser_error(
                        p, "Unknown key \"%.*s\"\n", key.len, key.data + key.off);
                }
                /* clang-format on */
                break;
            }

            default: return tok;
        }
    }
}

/* ------------------------------------------------------------------------- */
static int parse_ini(struct parser* p, struct settings* s)
{
    enum token tok;
    enum section
    {
        SEC_SERVER,
        SEC_WORLD,
        SEC_CLIENT,
        SEC_GFX,
        SEC_MCD
    };

    while (1)
    {
        tok = scan_next_token(p);
    reswitch_tok:
        switch (tok)
        {
            case TOK_ERROR: return -1;
            case TOK_END: return 0;

            case '[': {
                enum section sec;
                if (scan_next_token(p) != TOK_KEY)
                    return parser_error(
                        p,
                        "Expected a section name within the brackets. Example: "
                        "[server]\n");

                if (strview_eq_cstr(p->value.string, "server"))
                    sec = SEC_SERVER;
                else if (strview_eq_cstr(p->value.string, "world"))
                    sec = SEC_WORLD;
                else if (strview_eq_cstr(p->value.string, "client"))
                    sec = SEC_CLIENT;
                else if (strview_eq_cstr(p->value.string, "gfx"))
                    sec = SEC_GFX;
                else if (strview_eq_cstr(p->value.string, "mcd"))
                    sec = SEC_MCD;
                else
                    return parser_error(
                        p,
                        "Unknown section \"%.*s\". Expected [server]\n",
                        p->value.string.len,
                        p->source + p->value.string.off);

                if (scan_next_token(p) != ']')
                    return parser_error(p, "Missing closing bracket \"]\"\n");

                switch (sec)
                {
                    case SEC_SERVER:
                        tok = parse_server_key_values(p, &s->server);
                        break;
                    case SEC_WORLD:
                        tok = parse_world_key_values(p, &s->world);
                        break;
                    case SEC_CLIENT:
                        tok = parse_client_key_values(p, &s->client);
                        break;
                    case SEC_GFX: tok = parse_gfx_key_values(p, &s->gfx); break;
                    case SEC_MCD: tok = parse_mcd_key_values(p, &s->mcd); break;
                }
                goto reswitch_tok;
            }

            default:
                return parser_error(
                    p, "Expected to find a section. Example: [server]\n");
        }
    }
}

/* ------------------------------------------------------------------------- */
int settings_load(struct settings* s, const char* filepath)
{
    struct mfile  mf;
    struct parser p;

    settings_set_defaults(s);

    if (mfile_map_read(&mf, filepath, 1) != 0)
    {
        log_warn(
            "Using default settings and saving them to \"%s\"\n", filepath);
        settings_save(s, filepath);
        return 0;
    }

    parser_init(&p, &mf, filepath);
    if (parse_ini(&p, s) != 0)
        goto parser_error;

    mfile_unmap(&mf);
    return 0;

parser_error:
    mfile_unmap(&mf);
    return -1;
}

/* ------------------------------------------------------------------------- */
void settings_save(const struct settings* s, const char* filename)
{
    FILE*                         fp;
    const struct settings_server* sv = &s->server;
    const struct settings_world*  w = &s->world;
    const struct settings_client* cl = &s->client;
    const struct settings_gfx*    gfx = &s->gfx;
    const struct settings_mcd*    mcd = &s->mcd;

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

    log_dbg("Saving settings to \"%s\"\n", filename);

#define WRITE_INT(fp, s, prop, desc)                                           \
    fprintf(fp, #prop " = %d ; " desc "\n", s->prop)
#define WRITE_STR(fp, s, prop, desc)                                           \
    fprintf(fp, #prop " = \"%s\" ; " desc "\n", s->prop)
#define WRITE_STR_AS_INT(fp, s, prop, desc)                                    \
    fprintf(fp, #prop " = %s ; " desc "\n", s->prop)

    /* clang-format off */
    fprintf(fp, "[server]\n");
    WRITE_INT(fp, sv, max_players, "Total number of clients allowed to join the server");
    WRITE_INT(fp, sv, client_timeout, "How many seconds to wait for a client before disconnecting them");
    WRITE_INT(fp, sv, malicious_timeout, "How many seconds to keep a client on the malicious list");
    WRITE_INT(fp, sv, max_username_len, "Limits the user name length");
    WRITE_INT(fp, sv, sim_tick_rate, "Simulation speed in Hz");
    WRITE_INT(fp, sv, net_tick_rate, "Network update speed in Hz. Should be smaller or equal to simulation speed");
    WRITE_STR(fp, sv, bind_addr, "Address to bind server to");
    WRITE_STR_AS_INT(fp, sv, bind_port, "Port to bind server to");
    WRITE_STR(fp, sv, log_prefix, "Prefix for log messages");

    fprintf(fp, "\n[world]\n");
    WRITE_INT(fp, w, inner_radius, "Inner radius of the world");
    WRITE_INT(fp, w, ring_start, "Distance to the start of the outer ring");
    WRITE_INT(fp, w, ring_end, "Distance to the end of the outer ring");

    fprintf(fp, "\n[client]\n");
    WRITE_STR(fp, cl, username, "Default username");
    WRITE_STR(fp, cl, connect_addr, "Address to connect to");
    WRITE_STR_AS_INT(fp, cl, connect_port, "Port to connect to");
    WRITE_STR(fp, cl, log_prefix, "Default username");

    fprintf(fp, "\n[gfx]\n");
    WRITE_INT(fp, gfx, width, "Window width");
    WRITE_INT(fp, gfx, height, "Window height");
    WRITE_INT(fp, gfx, backend, "Graphics backend to use");
    WRITE_INT(fp, gfx, enable, "Enable graphics");

    fprintf(fp, "\n[mcd]\n");
    WRITE_INT(fp, mcd, enable, "Enable McDonald's WiFi mode");
    WRITE_INT(fp, mcd, latency_ms, "Latency in milliseconds");
    WRITE_INT(fp, mcd, loss_percent, "Packet loss percentage");
    WRITE_INT(fp, mcd, dup_percent, "Packet duplication percentage");
    WRITE_INT(fp, mcd, reorder_percent, "Packet reordering percentage");
    WRITE_STR(fp, mcd, bind_addr, "Address to bind server to");
    WRITE_STR_AS_INT(fp, mcd, bind_port, "Port to bind server to");
    WRITE_STR(fp, mcd, connect_addr, "Address to connect to");
    WRITE_STR_AS_INT(fp, mcd, connect_port, "Port to connect to");
    /* clang-format on */

    fclose(fp);
}
