#include "clither/log.h"
#include "clither/mem.h"
#include "clither/mfile.h"
#include "clither/resource_pack.h"
#include "clither/str.h"
#include "clither/strlist.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

enum token
{
    TOK_ERROR = -1,
    TOK_END = 0,
    TOK_LBRACKET = '[',
    TOK_RBRACKET = ']',
    TOK_COMMA = ',',
    TOK_PERIOD = '.',
    TOK_EQUALS = '=',
    TOK_INTEGER = 256,
    TOK_FLOAT,
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

struct section
{
    struct strview section;
    struct strview subsection;
    struct strview subsubsection;

    int section_index;
};

static void section_init(struct section* s)
{
    s->section = strview("", 0, 0);
    s->subsection = strview("", 0, 0);
    s->subsubsection = strview("", 0, 0);
    s->section_index = -1;
}

/* ------------------------------------------------------------------------- */
static int print_verror(
    const char*    filename,
    const char*    source,
    struct strspan loc,
    const char*    fmt,
    va_list        ap)
{
    log_vflc(filename, source, loc, fmt, ap);
    log_excerpt(source, loc);
    return -1;
}

static int print_error(
    const char*    filename,
    const char*    source,
    struct strspan loc,
    const char*    fmt,
    ...)
{
    va_list ap;
    va_start(ap, fmt);
    print_verror(filename, source, loc, fmt, ap);
    va_end(ap);
    return -1;
}
static int parser_error(struct parser* p, const char* fmt, ...)
{
    va_list        ap;
    struct strspan loc = strspan(p->tail, p->head - p->tail);

    va_start(ap, fmt);
    print_verror(p->filename, p->source, loc, fmt, ap);
    va_end(ap);

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
        if (p->source[p->head] == ',')
            return p->source[p->head++];
        if (p->source[p->head] == '.')
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
static enum token parse_section_desc(struct parser* p, struct section* s)
{
    enum token tok;
    if (scan_next_token(p) != TOK_KEY)
        return parser_error(
            p,
            "Expected a section name within the brackets. Example: "
            "[section]\n");
    s->section = p->value.string;

    tok = scan_next_token(p);
    if (tok == TOK_INTEGER)
    {
        s->section_index = p->value.integer_literal;
        tok = scan_next_token(p);
    }

    if (tok != '.')
        return tok;
    if (scan_next_token(p) != TOK_KEY)
        return parser_error(
            p,
            "Expected a subsection name after the period. "
            "Example: [section.subsection]\n");
    s->subsection = p->value.string;

    tok = scan_next_token(p);
    if (tok != '.')
        return tok;
    if (scan_next_token(p) != TOK_KEY)
        return parser_error(
            p,
            "Expected a subsubsection name after the period. "
            "Example: [section.subsection.subsubsection]\n");
    s->subsubsection = p->value.string;

    return scan_next_token(p);
}

/* ------------------------------------------------------------------------- */
static int parse_section_glsl(struct parser* p, struct resource_pack* pack)
{
    return -1;
}

/* ------------------------------------------------------------------------- */
static int
parse_section_background(struct parser* p, struct resource_pack* pack)
{
    return -1;
}

/* ------------------------------------------------------------------------- */
static int parse_section(
    struct parser* p, const struct section* s, struct resource_pack* pack)
{
    if (strview_eq_cstr(s->section, "shaders"))
    {
        if (strview_eq_cstr(s->subsection, "glsl"))
            return parse_section_glsl(p, pack);

        if (s->subsection.len == 0)
            return print_error(
                p->filename,
                p->source,
                strspan(s->section.off, s->section.len),
                "Section 'shaders' requires a subsection for the shader "
                "language. Example: [shaders.glsl]\n");
        else
            return print_error(
                p->filename,
                p->source,
                strspan(s->subsection.off, s->subsection.len),
                "Unknown subsection '%.*s'\n",
                s->subsection.len,
                p->source + s->subsection.off);
    }

    if (strview_eq_cstr(s->section, "background"))
        return parse_section_background(p, pack);

    return print_error(
        p->filename,
        p->source,
        strspan(s->section.off, s->section.len),
        "Unknown section '%.*s'\n",
        s->section.len,
        p->source + s->section.off);
}

/* ------------------------------------------------------------------------- */
static int parse_ini(struct parser* p, struct resource_pack* pack)
{
    enum token tok;
    while (1)
    {
        switch ((tok = scan_next_token(p)))
        {
            case TOK_ERROR: return -1;
            case TOK_END: return 0;

            case '[': {
                struct section section;
                section_init(&section);
                if (parse_section_desc(p, &section) != ']')
                    return parser_error(p, "Missing closing bracket ']'\n");
                if (parse_section(p, &section, pack) != 0)
                    return -1;
                break;
            }

            default:
                return parser_error(
                    p,
                    "Unexpected token encountered. Expected a section name. "
                    "Example: [section.subsection]\n");
        }
    }
}

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
static struct resource_sprite* resource_sprite_create(void)
{
    struct resource_sprite* res = mem_alloc(sizeof *res);
    str_init(&res->texture0);
    str_init(&res->texture1);
    res->tile_x = 1;
    res->tile_y = 1;
    res->num_frames = 1;
    res->fps = 0;
    res->scale = 1.0;
    return res;
}

/* ------------------------------------------------------------------------- */
static void resource_sprite_destroy(struct resource_sprite* res)
{
    str_deinit(res->texture0);
    str_deinit(res->texture1);
    mem_free(res);
}

/* ------------------------------------------------------------------------- */
static struct resource_snake_part* resource_snake_part_create(void)
{
    struct resource_snake_part* part = mem_alloc(sizeof *part);
    memset(part, 0, sizeof *part);
    return part;
}

/* ------------------------------------------------------------------------- */
static void resource_snake_part_destroy(struct resource_snake_part* part)
{
    if (part->base)
        resource_sprite_destroy(part->base);
    if (part->gather)
        resource_sprite_destroy(part->gather);
    if (part->boost)
        resource_sprite_destroy(part->boost);
    if (part->turn)
        resource_sprite_destroy(part->turn);
    if (part->projectile)
        resource_sprite_destroy(part->projectile);
    if (part->split)
        resource_sprite_destroy(part->split);
    if (part->armor)
        resource_sprite_destroy(part->armor);
    mem_free(part);
}

/* ------------------------------------------------------------------------- */
static void resource_pack_init(struct resource_pack* pack)
{
    strlist_init(&pack->shaders.glsl.background);
    strlist_init(&pack->shaders.glsl.shadow);
    strlist_init(&pack->shaders.glsl.sprite);
    resource_sprite_vec_init(&pack->sprites.background);
    resource_snake_part_vec_init(&pack->sprites.head);
    resource_snake_part_vec_init(&pack->sprites.body);
    resource_snake_part_vec_init(&pack->sprites.tail);
}

/* ------------------------------------------------------------------------- */
static void resource_pack_deinit(struct resource_pack* pack)
{
    resource_snake_part_vec_deinit(pack->sprites.tail);
    resource_snake_part_vec_deinit(pack->sprites.body);
    resource_snake_part_vec_deinit(pack->sprites.head);
    resource_sprite_vec_deinit(pack->sprites.background);
    strlist_deinit(pack->shaders.glsl.sprite);
    strlist_deinit(pack->shaders.glsl.shadow);
    strlist_deinit(pack->shaders.glsl.background);
}

/* ------------------------------------------------------------------------- */
struct resource_pack* resource_pack_parse(const char* pack_path)
{
    struct mfile          mf;
    struct str*           str;
    struct resource_pack* pack;
    struct parser         p;

    str_init(&str);
    if (str_set_cstr(&str, pack_path) != 0)
        goto open_config_ini_failed;
    if (str_join_path_cstr(&str, "config.ini") != 0)
        goto open_config_ini_failed;
    log_dbg("Reading file \"%s\"\n", str_cstr(str));
    if (mfile_map_read(&mf, str_cstr(str), 1))
        goto open_config_ini_failed;

    pack = mem_alloc(sizeof *pack);
    if (pack == NULL)
        goto alloc_pack_failed;
    resource_pack_init(pack);

    parser_init(&p, &mf, str_cstr(str));
    if (parse_ini(&p, pack) != 0)
        goto parse_error;

    mfile_unmap(&mf);
    str_deinit(str);

    return pack;

parse_error:
    resource_pack_deinit(pack);
alloc_pack_failed:
    mfile_unmap(&mf);
open_config_ini_failed:
    str_deinit(str);
    return NULL;
}

/* ------------------------------------------------------------------------- */
void resource_pack_destroy(struct resource_pack* pack)
{
    resource_pack_deinit(pack);
    mem_free(pack);
}
