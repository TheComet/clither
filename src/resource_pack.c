#include "clither/log.h"
#include "clither/mem.h"
#include "clither/mfile.h"
#include "clither/resource_pack.h"
#include "clither/resource_snake_part_vec.h"
#include "clither/resource_sprite_vec.h"
#include "clither/str.h"
#include "clither/strlist.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
static struct resource_sprite* resource_sprite_create(void)
{
    struct resource_sprite* sprite = mem_alloc(sizeof *sprite);
    if (sprite == NULL)
    {
        log_oom(sizeof *sprite, "resource_sprite_create()");
        return NULL;
    }

    strlist_init(&sprite->textures);
    sprite->tile_x = 1;
    sprite->tile_y = 1;
    sprite->num_frames = 1;
    sprite->fps = 0;
    sprite->scale = 1.0;

    return sprite;
}

/* ------------------------------------------------------------------------- */
static void resource_sprite_destroy(struct resource_sprite* sprite)
{
    strlist_deinit(sprite->textures);
    mem_free(sprite);
}

/* ------------------------------------------------------------------------- */
static void resource_snake_part_init(struct resource_snake_part* part)
{
    memset(part, 0, sizeof *part);
}

/* ------------------------------------------------------------------------- */
static void resource_snake_part_deinit(struct resource_snake_part* part)
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
}

/* ------------------------------------------------------------------------- */
static void resource_pack_init(struct resource_pack* pack)
{
    strlist_init(&pack->shaders.glsl.background);
    strlist_init(&pack->shaders.glsl.shadow);
    strlist_init(&pack->shaders.glsl.sprite);
    resource_sprite_vec_init(&pack->sprites.background);
    resource_snake_part_vec_init(&pack->sprites.heads);
    resource_snake_part_vec_init(&pack->sprites.bodies);
    resource_snake_part_vec_init(&pack->sprites.tails);
}

/* ------------------------------------------------------------------------- */
static void resource_pack_deinit(struct resource_pack* pack)
{
    struct resource_snake_part* snake_part;
    struct resource_sprite**    sprite;

    vec_for_each (pack->sprites.tails, snake_part)
        resource_snake_part_deinit(snake_part);
    resource_snake_part_vec_deinit(pack->sprites.tails);

    vec_for_each (pack->sprites.bodies, snake_part)
        resource_snake_part_deinit(snake_part);
    resource_snake_part_vec_deinit(pack->sprites.bodies);

    vec_for_each (pack->sprites.heads, snake_part)
        resource_snake_part_deinit(snake_part);
    resource_snake_part_vec_deinit(pack->sprites.heads);

    vec_for_each (pack->sprites.background, sprite)
        if (*sprite)
            resource_sprite_destroy(*sprite);
    resource_sprite_vec_deinit(pack->sprites.background);

    strlist_deinit(pack->shaders.glsl.sprite);
    strlist_deinit(pack->shaders.glsl.shadow);
    strlist_deinit(pack->shaders.glsl.background);
}

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
            while (p->head != p->end && isalnum(p->source[p->head]))
                p->head++;
            p->value.string = strview(p->source, p->tail, p->head - p->tail);
            return TOK_KEY;
        }

        /* Ignore everything else */
        p->tail = ++p->head;
    }

    return TOK_END;
}

/* ------------------------------------------------------------------------- */
static int parse_string_list(
    struct parser* p, struct strlist** list, const char* path_prefix)
{
    enum token  tok;
    struct str* str;
    str_init(&str);

scan_next_string:
    tok = scan_next_token(p);
    if (tok != TOK_STRING)
        return parser_error(p, "Expected a string value\n");

    if (str_set_cstr(&str, path_prefix) != 0)
        goto error;
    if (str_join_path(&str, p->value.string) != 0)
        goto error;
    if (strlist_add_cstr(list, str_cstr(str)) != 0)
        goto error;

    tok = scan_next_token(p);
    if (tok == ',')
        goto scan_next_string;

    str_deinit(str);
    return tok;

error:
    str_deinit(str);
    return -1;
}

/* ------------------------------------------------------------------------- */
static int parse_section_glsl(
    struct parser* p, struct resource_pack* pack, const char* path_prefix)
{
    enum token tok;
    while (1)
    {
        tok = scan_next_token(p);
    reswitch_tok:
        switch (tok)
        {
            case TOK_ERROR: return -1;
            case TOK_END: return 0;

            case TOK_KEY: {
                struct strlist** shaderlist;
                struct strview   key = p->value.string;
                if (strview_eq_cstr(key, "shadow"))
                    shaderlist = &pack->shaders.glsl.shadow;
                else if (strview_eq_cstr(key, "sprite"))
                    shaderlist = &pack->shaders.glsl.sprite;
                else if (strview_eq_cstr(key, "background"))
                    shaderlist = &pack->shaders.glsl.background;
                else
                    return parser_error(
                        p,
                        "Unknown key \"%.*s\"\n",
                        key.len,
                        key.data + key.off);

                if (scan_next_token(p) != '=')
                    return parser_error(p, "Expected '=' after key\n");
                tok = parse_string_list(p, shaderlist, path_prefix);
                goto reswitch_tok;
            }

            default: return tok;
        }
    }
}

/* ------------------------------------------------------------------------- */
static int parse_section_sprite(
    struct parser* p, struct resource_sprite* sprite, const char* path_prefix)
{
    enum token tok;
    while (1)
    {
        tok = scan_next_token(p);
    reswitch_tok:
        switch (tok)
        {
            case TOK_ERROR: return -1;
            case TOK_END: return 0;

            case TOK_KEY: {
                struct strview key = p->value.string;

                if (strview_eq_cstr(key, "textures"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    tok = parse_string_list(p, &sprite->textures, path_prefix);
                    goto reswitch_tok;
                }

                if (strview_eq_cstr(key, "tile"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    if (scan_next_token(p) != TOK_INTEGER)
                        return parser_error(
                            p,
                            "Expected an integer value. Example: tile = 4,4\n");
                    sprite->tile_x = p->value.integer_literal;
                    if (scan_next_token(p) != ',')
                        return parser_error(
                            p, "Expected a comma after the first tile value\n");
                    if (scan_next_token(p) != TOK_INTEGER)
                        return parser_error(
                            p,
                            "Expected an integer value. Example: tile = 4,4\n");
                    sprite->tile_y = p->value.integer_literal;
                    break;
                }

                if (strview_eq_cstr(key, "frames"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    if (scan_next_token(p) != TOK_INTEGER)
                        return parser_error(
                            p,
                            "Expected an integer value. Example: frames = "
                            "16\n");
                    sprite->num_frames = p->value.integer_literal;
                    break;
                }

                if (strview_eq_cstr(key, "fps"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    if (scan_next_token(p) != TOK_INTEGER)
                        return parser_error(
                            p,
                            "Expected an integer value. Example: fps = 20\n");
                    sprite->fps = p->value.integer_literal;
                    break;
                }

                if (strview_eq_cstr(key, "scale"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    if (scan_next_token(p) != TOK_FLOAT)
                        return parser_error(
                            p,
                            "Expected a float value. Example: scale = 2.0\n");
                    sprite->scale = p->value.float_literal;
                    break;
                }

                return parser_error(
                    p, "Unknown key \"%.*s\"\n", key.len, key.data + key.off);
            }

            default: return tok;
        }
    }
}

/* ------------------------------------------------------------------------- */
static void parse_section_name_and_index(struct section* s, struct strview key)
{
    int first_digit = key.off + key.len;
    while (first_digit > 0 && isdigit(key.data[first_digit - 1]))
        first_digit--;

    s->section = strview(key.data, key.off, first_digit - key.off);
    s->section_index =
        first_digit < key.off + key.len
            ? strview_to_integer(strview(
                  key.data, first_digit, key.off + key.len - first_digit))
            : -1;
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
    parse_section_name_and_index(s, p->value.string);

    tok = scan_next_token(p);
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
static int parse_section(
    struct parser*        p,
    const struct section* s,
    struct resource_pack* pack,
    const char*           path_prefix)
{
    if (strview_eq_cstr(s->section, "shaders"))
    {
        if (strview_eq_cstr(s->subsection, "glsl"))
            return parse_section_glsl(p, pack, path_prefix);

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
                "Unknown subsection \"%.*s\"\n",
                s->subsection.len,
                p->source + s->subsection.off);
    }

    if (strview_eq_cstr(s->section, "background"))
    {
        struct resource_sprite** bg;

        if (s->section_index < 0)
            return print_error(
                p->filename,
                p->source,
                strspan(s->section.off, s->section.len),
                "Section 'background' requires an index. Example: "
                "[background0]\n");

        while (vec_count(pack->sprites.background) <= s->section_index)
        {
            if (resource_sprite_vec_push(&pack->sprites.background, NULL) != 0)
                return -1;
        }

        bg = vec_get(pack->sprites.background, s->section_index);
        if (*bg == NULL)
        {
            *bg = resource_sprite_create();
            if (*bg == NULL)
                return -1;
        }
        return parse_section_sprite(p, *bg, path_prefix);
    }

    if (strview_eq_cstr(s->section, "snake"))
    {
        struct resource_snake_part_vec** snake_part_vec;
        struct resource_snake_part*      snake_part;
        struct resource_sprite**         sprite;
        if (s->section_index < 0)
            return print_error(
                p->filename,
                p->source,
                strspan(s->section.off, s->section.len),
                "Section 'snake' requires an index. Example: "
                "[snake0.head.base]\n");

        if (strview_eq_cstr(s->subsection, "head"))
            snake_part_vec = &pack->sprites.heads;
        else if (strview_eq_cstr(s->subsection, "body"))
            snake_part_vec = &pack->sprites.bodies;
        else if (strview_eq_cstr(s->subsection, "tail"))
            snake_part_vec = &pack->sprites.tails;
        else
            return print_error(
                p->filename,
                p->source,
                strspan(s->subsection.off, s->subsection.len),
                "Unknown snake part \"%.*s\". Know 'head', 'body', and "
                "'tail'\n",
                s->subsection.len,
                p->source + s->subsection.off);

        while (vec_count(pack->sprites.heads) <= s->section_index)
        {
            snake_part = resource_snake_part_vec_emplace(&pack->sprites.heads);
            if (snake_part == NULL)
                return -1;
            resource_snake_part_init(snake_part);
        }
        while (vec_count(pack->sprites.bodies) <= s->section_index)
        {
            snake_part = resource_snake_part_vec_emplace(&pack->sprites.bodies);
            if (snake_part == NULL)
                return -1;
            resource_snake_part_init(snake_part);
        }
        while (vec_count(pack->sprites.tails) <= s->section_index)
        {
            snake_part = resource_snake_part_vec_emplace(&pack->sprites.tails);
            if (snake_part == NULL)
                return -1;
            resource_snake_part_init(snake_part);
        }
        snake_part = vec_get(*snake_part_vec, s->section_index);

        if (strview_eq_cstr(s->subsubsection, "base"))
            sprite = &snake_part->base;
        else if (strview_eq_cstr(s->subsubsection, "gather"))
            sprite = &snake_part->gather;
        else if (strview_eq_cstr(s->subsubsection, "boost"))
            sprite = &snake_part->boost;
        else if (strview_eq_cstr(s->subsubsection, "turn"))
            sprite = &snake_part->turn;
        else if (strview_eq_cstr(s->subsubsection, "projectile"))
            sprite = &snake_part->projectile;
        else if (strview_eq_cstr(s->subsubsection, "split"))
            sprite = &snake_part->split;
        else if (strview_eq_cstr(s->subsubsection, "armor"))
            sprite = &snake_part->armor;
        else
            return print_error(
                p->filename,
                p->source,
                strspan(s->subsubsection.off, s->subsubsection.len),
                "Unknown snake part \"%.*s\". Know 'base', 'gather', 'boost', "
                "'turn', 'projectile', 'split', and 'armor'\n",
                s->subsubsection.len,
                p->source + s->subsubsection.off);

        if (*sprite == NULL)
            *sprite = resource_sprite_create();
        if (*sprite == NULL)
            return -1;

        return parse_section_sprite(p, *sprite, path_prefix);
    }

    return print_error(
        p->filename,
        p->source,
        strspan(s->section.off, s->section.len),
        "Unknown section \"%.*s\"\n",
        s->section.len,
        p->source + s->section.off);
}

/* ------------------------------------------------------------------------- */
static int
parse_ini(struct parser* p, struct resource_pack* pack, const char* path_prefix)
{
    enum token tok;
    while (1)
    {
        tok = scan_next_token(p);
    reswitch_tok:
        switch (tok)
        {
            case TOK_ERROR: return -1;
            case TOK_END: return 0;

            case '[': {
                struct section section;
                section_init(&section);
                if (parse_section_desc(p, &section) != ']')
                    return parser_error(p, "Missing closing bracket ']'\n");
                tok = parse_section(p, &section, pack, path_prefix);
                goto reswitch_tok;
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
    if (parse_ini(&p, pack, pack_path) != 0)
        goto parse_error;

    mfile_unmap(&mf);
    str_deinit(str);

    return pack;

parse_error:
    resource_pack_deinit(pack);
    mem_free(pack);
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
