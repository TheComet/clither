#include "clither/audio/audio.h"
#include "clither/game/resource_pack.h"
#include "clither/platform/fs.h"
#include "clither/platform/mfile.h"
#include "clither/util/log.h"
#include "clither/util/mem.h"
#include "clither/util/str.h"
#include "clither/util/strlist.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

HMAP_DEFINE_STR(extern, resource_shader_hmap, struct resource_shader, 16)
HMAP_DEFINE_STR(extern, resource_sprite_hmap, struct resource_sprite, 16)
HMAP_DEFINE_STR(extern, resource_snake_hmap, struct resource_snake, 16)
HMAP_DEFINE_STR(extern, resource_spine_hmap, struct resource_spine, 16)

#define HANDLE_STR(name, key, resource)                                        \
    if (strview_eq_cstr(key, #name))                                           \
    {                                                                          \
        if (scan_next_token(p) != '=')                                         \
            return parser_error(p, "Expected '=' after \"" #name "\"\n");      \
        if (scan_next_token(p) != TOK_STRING)                                  \
            return parser_error(p, "Expected a string value\n");               \
        if (str_set_view(&food->name, p->value.string) != 0)                   \
            return -1;                                                         \
        break;                                                                 \
    }

#define HANDLE_FLOAT(name, key, resource)                                      \
    if (strview_eq_cstr(key, #name))                                           \
    {                                                                          \
        if (scan_next_token(p) != '=')                                         \
            return parser_error(p, "Expected '=' after \"" #name "\"\n");      \
        if (scan_next_token(p) != TOK_FLOAT)                                   \
            return parser_error(                                               \
                p, "Expected a float value. Example: scale = 2.0\n");          \
        food->scale = p->value.float_literal;                                  \
        break;                                                                 \
    }

#define HANDLE_PATH(name, key, resource, path_prefix)                          \
    if (strview_eq_cstr(key, #name))                                           \
    {                                                                          \
        if (scan_next_token(p) != '=')                                         \
            return parser_error(p, "Expected '=' after \"" #name "\"\n");      \
        if (scan_next_token(p) != TOK_STRING)                                  \
            return parser_error(p, "Expected a string value\n");               \
        if (str_set_cstr(&resource->name, path_prefix) != 0)                   \
            return -1;                                                         \
        if (str_join_path(&resource->name, p->value.string) != 0)              \
            return -1;                                                         \
        break;                                                                 \
    }

/* ------------------------------------------------------------------------- */
static void resource_background_init(struct resource_background* res)
{
    strlist_init(&res->textures);
}

/* ------------------------------------------------------------------------- */
static void resource_background_deinit(struct resource_background* res)
{
    strlist_deinit(res->textures);
}

/* ------------------------------------------------------------------------- */
static void resource_text_init(struct resource_text* res)
{
    str_init(&res->font);
    res->size = 72;
    res->dpi = 72;
}

/* ------------------------------------------------------------------------- */
static void resource_text_deinit(struct resource_text* res)
{
    str_deinit(res->font);
}

/* ------------------------------------------------------------------------- */
static void resource_layer_init(struct resource_layer* res)
{
    strlist_init(&res->textures);
    res->tile_x = 1;
    res->tile_y = 1;
    res->num_frames = 1;
    res->fps = 0;
}

/* ------------------------------------------------------------------------- */
static void resource_layer_deinit(struct resource_layer* res)
{
    strlist_deinit(res->textures);
}

/* ------------------------------------------------------------------------- */
static void resource_sprite_init(struct resource_sprite* res)
{
    int i;
    for (i = 0; i != RESOURCE_LAYER_COUNT; i++)
        resource_layer_init(&res->layer[i]);
}

/* ------------------------------------------------------------------------- */
static void resource_sprite_deinit(struct resource_sprite* res)
{
    int i;
    for (i = 0; i != RESOURCE_LAYER_COUNT; i++)
        resource_layer_deinit(&res->layer[i]);
}

/* ------------------------------------------------------------------------- */
static void resource_spine_init(struct resource_spine* res)
{
    strlist_init(&res->textures);
}

/* ------------------------------------------------------------------------- */
static void resource_spine_deinit(struct resource_spine* res)
{
    strlist_deinit(res->textures);
}

/* ------------------------------------------------------------------------- */
static void resource_food_init(struct resource_food* res)
{
    str_init(&res->sprite);
    res->scale = 1.0;
}

/* ------------------------------------------------------------------------- */
static void resource_food_deinit(struct resource_food* res)
{
    str_deinit(res->sprite);
}

/* ------------------------------------------------------------------------- */
static void resource_audio_init(struct resource_audio* res)
{
    str_init(&res->menu_music);

    str_init(&res->button_hover);
    str_init(&res->button_click);
    str_init(&res->button_back);

    str_init(&res->slider_click);
    str_init(&res->slider_drag);
    str_init(&res->slider_release);

    str_init(&res->textinput_type);
    str_init(&res->textinput_delete);

    str_init(&res->eat_food);
}

/* ------------------------------------------------------------------------- */
static void resource_audio_deinit(struct resource_audio* res)
{
    str_deinit(res->eat_food);

    str_deinit(res->textinput_delete);
    str_deinit(res->textinput_type);

    str_deinit(res->slider_release);
    str_deinit(res->slider_drag);
    str_deinit(res->slider_click);

    str_deinit(res->button_back);
    str_deinit(res->button_click);
    str_deinit(res->button_hover);

    str_deinit(res->menu_music);
}

/* ------------------------------------------------------------------------- */
static void resource_snake_init(struct resource_snake* res)
{
    str_init(&res->head_sprite);
    str_init(&res->tail_sprite);
    strlist_init(&res->body_sprites);
    str_init(&res->spine);
}

/* ------------------------------------------------------------------------- */
static void resource_snake_deinit(struct resource_snake* res)
{
    str_deinit(res->spine);
    strlist_deinit(res->body_sprites);
    str_deinit(res->tail_sprite);
    str_deinit(res->head_sprite);
}

/* ------------------------------------------------------------------------- */
static void resource_pack_init(struct resource_pack* pack)
{
    pack->path = "";
    str_init(&pack->pack_ini);

    resource_background_init(&pack->background);
    resource_text_init(&pack->text);
    resource_food_init(&pack->food);
    resource_audio_init(&pack->audio);

    resource_spine_hmap_init(&pack->spines);
    resource_shader_hmap_init(&pack->shaders);
    resource_sprite_hmap_init(&pack->sprites);
    resource_snake_hmap_init(&pack->snakes);
}

/* ------------------------------------------------------------------------- */
static void resource_pack_deinit(struct resource_pack* pack)
{
    int16_t                 slot;
    struct str*             name;
    struct resource_snake*  snake;
    struct resource_sprite* sprite;
    struct resource_shader* shader;
    struct resource_spine*  spine;

    hmap_for_each (pack->snakes, slot, name, snake)
        (void)slot, (void)name, resource_snake_deinit(snake);
    resource_snake_hmap_deinit(pack->snakes);

    hmap_for_each (pack->sprites, slot, name, sprite)
        (void)slot, (void)name, resource_sprite_deinit(sprite);
    resource_sprite_hmap_deinit(pack->sprites);

    hmap_for_each (pack->shaders, slot, name, shader)
        (void)slot, (void)name, resource_shader_deinit(shader);
    resource_shader_hmap_deinit(pack->shaders);

    hmap_for_each (pack->spines, slot, name, spine)
        (void)slot, (void)name, resource_spine_deinit(spine);
    resource_spine_hmap_deinit(pack->spines);

    resource_audio_deinit(&pack->audio);
    resource_food_deinit(&pack->food);
    resource_text_deinit(&pack->text);
    resource_background_deinit(&pack->background);

    str_deinit(pack->pack_ini);
}

/* ------------------------------------------------------------------------- */
enum token
{
    TOK_ERROR = -1,
    TOK_END = 0,
    TOK_LBRACKET = '[',
    TOK_RBRACKET = ']',
    TOK_COMMA = ',',
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
            while (p->head != p->end &&
                   (isalnum(p->source[p->head]) || p->source[p->head] == '_'))
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

    if (path_prefix)
    {
        if (str_set_cstr(&str, path_prefix) != 0)
            goto error;
        if (str_join_path(&str, p->value.string) != 0)
            goto error;
    }
    else
    {
        if (str_set_view(&str, p->value.string) != 0)
            goto error;
    }

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
static int on_section_shader(struct c_ini_parser* p, void* user_ptr)
{
    int                     result;
    struct resource_shader  s;
    struct resource_pack*   pack = user_ptr;
    struct resource_shader* inserted = NULL;

    resource_shader_init(&s);
    result = resource_shader_parse_section(&s, p);
    if (result < 0)
        goto failed;

    switch (resource_shader_hmap_emplace_or_get(
        &pack->shaders, str_view(s.target), &inserted))
    {
        case HMAP_OOM: return -1;
        case HMAP_NEW: *inserted = s; break;
        case HMAP_EXISTS:
            log_err(
                "Shader target \"%s\" already exists\n", str_cstr(s.target));
            goto failed;
    }

    return result;

failed:
    resource_shader_deinit(&s);
    return -1;
}

static int parse_section_shader(
    struct parser*                p,
    struct resource_shader_hmap** shaders,
    const char*                   path_prefix)
{
    enum token tok;

    while (1)
    {
        tok = scan_next_token(p);
        switch (tok)
        {
            case TOK_ERROR: return -1;
            case TOK_END: return 0;
            case '[': return tok;
            default: continue;
        }
    }
}

/* ------------------------------------------------------------------------- */
static int parse_section_background(
    struct parser* p, struct resource_background* bg, const char* path_prefix)
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
                    tok = parse_string_list(p, &bg->textures, path_prefix);
                    goto reswitch_tok;
                }

                return parser_error(
                    p, "Unknown key \"%.*s\"\n", key.len, key.data + key.off);
            }

            default: return tok;
        }
    }
}

/* ------------------------------------------------------------------------- */
enum token parse_section_text(
    struct parser* p, struct resource_text* text, const char* path_prefix)
{
    enum token tok;
    while (1)
    {
        tok = scan_next_token(p);
        switch (tok)
        {
            case TOK_ERROR: return -1;
            case TOK_END: return 0;

            case TOK_KEY: {
                struct strview key = p->value.string;

                if (strview_eq_cstr(key, "font"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    if (scan_next_token(p) != TOK_STRING)
                        return parser_error(p, "Expected a string value\n");
                    if (str_set_cstr(&text->font, path_prefix) != 0)
                        return -1;
                    if (str_join_path(&text->font, p->value.string) != 0)
                        return -1;
                    break;
                }

                if (strview_eq_cstr(key, "size"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    if (scan_next_token(p) != TOK_INTEGER)
                        return parser_error(
                            p,
                            "Expected an integer value. Example: size = 72\n");
                    text->size = p->value.integer_literal;
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
enum token parse_section_food(struct parser* p, struct resource_food* food)
{
    enum token tok;
    while (1)
    {
        tok = scan_next_token(p);
        switch (tok)
        {
            case TOK_ERROR: return -1;
            case TOK_END: return 0;

            case TOK_KEY: {
                struct strview key = p->value.string;

                HANDLE_STR(sprite, key, food)
                HANDLE_FLOAT(scale, key, food)

                return parser_error(
                    p, "Unknown key \"%.*s\"\n", key.len, key.data + key.off);
            }

            default: return tok;
        }
    }
}

/* ------------------------------------------------------------------------- */
static enum token parse_section_audio(
    struct parser* p, struct resource_audio* audio, const char* path_prefix)
{
    enum token tok;
    while (1)
    {
        tok = scan_next_token(p);
        switch (tok)
        {
            case TOK_ERROR: return -1;
            case TOK_END: return 0;

            case TOK_KEY: {
                struct strview key = p->value.string;
                HANDLE_PATH(menu_music, key, audio, path_prefix)

#define X(name, NAME) HANDLE_PATH(name, key, audio, path_prefix)
                AUDIO_SFX_LIST
#undef X

                return parser_error(
                    p, "Unknown key \"%.*s\"\n", key.len, key.data + key.off);
            }

            default: return tok;
        }
    }
}

/* ------------------------------------------------------------------------- */
static int parse_section_sprite(
    struct parser*                p,
    struct resource_sprite_hmap** sprites,
    const char*                   path_prefix)
{
    enum token              tok;
    struct resource_sprite* sprite = NULL;
    struct resource_layer*  layer = NULL;

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

                if (strview_eq_cstr(key, "name"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    if (scan_next_token(p) != TOK_STRING)
                        return parser_error(p, "Expected a string value\n");
                    switch (resource_sprite_hmap_emplace_or_get(
                        sprites, p->value.string, &sprite))
                    {
                        case HMAP_OOM: return -1;
                        case HMAP_NEW: resource_sprite_init(sprite);
                        case HMAP_EXISTS: break;
                    }
                    continue;
                }

                if (sprite == NULL)
                    return parser_error(
                        p,
                        "You need to specify the sprite name first. "
                        "Example:\n"
                        "name = \"metal head\"\n");

                if (strview_eq_cstr(key, "layer"))
                {
                    struct strview layer_name;
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    if (scan_next_token(p) != TOK_STRING)
                        return parser_error(p, "Expected a string value\n");
                    layer_name = p->value.string;
                    if (strview_eq_cstr(layer_name, "base"))
                        layer = &sprite->layer[RESOURCE_LAYER_BASE];
                    else if (strview_eq_cstr(layer_name, "gather"))
                        layer = &sprite->layer[RESOURCE_LAYER_GATHER];
                    else if (strview_eq_cstr(layer_name, "boost"))
                        layer = &sprite->layer[RESOURCE_LAYER_BOOST];
                    else if (strview_eq_cstr(layer_name, "turn"))
                        layer = &sprite->layer[RESOURCE_LAYER_TURN];
                    else if (strview_eq_cstr(layer_name, "projectile"))
                        layer = &sprite->layer[RESOURCE_LAYER_PROJECTILE];
                    else if (strview_eq_cstr(layer_name, "split"))
                        layer = &sprite->layer[RESOURCE_LAYER_SPLIT];
                    else if (strview_eq_cstr(layer_name, "armor"))
                        layer = &sprite->layer[RESOURCE_LAYER_ARMOR];
                    else
                        return parser_error(
                            p,
                            "Unknown layer \"%.*s\"\n",
                            layer_name.len,
                            layer_name.data + layer_name.off);
                    if (strlist_count(layer->textures) > 0)
                        return parser_error(
                            p,
                            "Layer \"%.*s\" was already defined for this "
                            "sprite\n",
                            layer_name.len,
                            layer_name.data + layer_name.off);
                    continue;
                }

                if (layer == NULL)
                    return parser_error(
                        p,
                        "You need to specify the layer first. "
                        "Example:\n"
                        "layer = \"base\"\n");

                if (strview_eq_cstr(key, "textures"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    tok = parse_string_list(p, &layer->textures, path_prefix);
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
                    layer->tile_x = p->value.integer_literal;
                    if (scan_next_token(p) != ',')
                        return parser_error(
                            p, "Expected a comma after the first tile value\n");
                    if (scan_next_token(p) != TOK_INTEGER)
                        return parser_error(
                            p,
                            "Expected an integer value. Example: tile = 4,4\n");
                    layer->tile_y = p->value.integer_literal;
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
                    layer->num_frames = p->value.integer_literal;
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
                    layer->fps = p->value.integer_literal;
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
static int parse_section_spine(
    struct parser*               p,
    struct resource_spine_hmap** spines,
    const char*                  path_prefix)
{
    enum token             tok;
    struct resource_spine* spine = NULL;

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

                if (strview_eq_cstr(key, "name"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    if (scan_next_token(p) != TOK_STRING)
                        return parser_error(p, "Expected a string value\n");
                    switch (resource_spine_hmap_emplace_or_get(
                        spines, p->value.string, &spine))
                    {
                        case HMAP_OOM: return -1;
                        case HMAP_NEW: resource_spine_init(spine); break;
                        case HMAP_EXISTS:
                            return parser_error(
                                p,
                                "Spine name \"%.*s\" already exists\n",
                                key.len,
                                key.data + key.off);
                    }
                    continue;
                }

                if (spine == NULL)
                    return parser_error(
                        p,
                        "You need to specify the spine name first. "
                        "Example:\n"
                        "name = \"metal spine\"\n");

                if (strview_eq_cstr(key, "textures"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    tok = parse_string_list(p, &spine->textures, path_prefix);
                    goto reswitch_tok;
                }

                return parser_error(
                    p, "Unknown key \"%.*s\"\n", key.len, key.data + key.off);
            }

            default: return tok;
        }
    }
}

/* ------------------------------------------------------------------------- */
static int
parse_section_snake(struct parser* p, struct resource_snake_hmap** snakes)
{
    enum token             tok;
    struct resource_snake* snake = NULL;

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

                if (strview_eq_cstr(key, "name"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    if (scan_next_token(p) != TOK_STRING)
                        return parser_error(p, "Expected a string value\n");
                    switch (resource_snake_hmap_emplace_or_get(
                        snakes, p->value.string, &snake))
                    {
                        case HMAP_OOM: return -1;
                        case HMAP_NEW: resource_snake_init(snake); break;
                        case HMAP_EXISTS:
                            return parser_error(
                                p,
                                "Snake name \"%.*s\" already exists\n",
                                key.len,
                                key.data + key.off);
                    }
                    continue;
                }

                if (snake == NULL)
                    return parser_error(
                        p,
                        "You need to specify the spine name first. "
                        "Example:\n"
                        "name = \"metal spine\"\n");

                if (strview_eq_cstr(key, "head_sprite"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    if (scan_next_token(p) != TOK_STRING)
                        return parser_error(p, "Expected a string value\n");
                    if (str_set_view(&snake->head_sprite, p->value.string) != 0)
                        return -1;
                    break;
                }

                if (strview_eq_cstr(key, "tail_sprite"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    if (scan_next_token(p) != TOK_STRING)
                        return parser_error(p, "Expected a string value\n");
                    if (str_set_view(&snake->tail_sprite, p->value.string) != 0)
                        return -1;
                    break;
                }

                if (strview_eq_cstr(key, "body_sprites"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    tok = parse_string_list(p, &snake->body_sprites, NULL);
                    goto reswitch_tok;
                }

                if (strview_eq_cstr(key, "spine"))
                {
                    if (scan_next_token(p) != '=')
                        return parser_error(p, "Expected '=' after key\n");
                    if (scan_next_token(p) != TOK_STRING)
                        return parser_error(p, "Expected a string value\n");
                    if (str_set_view(&snake->spine, p->value.string) != 0)
                        return -1;
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
static int parse_section(
    struct parser*        p,
    struct strview        section,
    struct resource_pack* pack,
    const char*           path_prefix)
{
    if (strview_eq_cstr(section, "shader"))
        return parse_section_shader(p, &pack->shaders, path_prefix);
    if (strview_eq_cstr(section, "background"))
        return parse_section_background(p, &pack->background, path_prefix);
    if (strview_eq_cstr(section, "text"))
        return parse_section_text(p, &pack->text, path_prefix);
    if (strview_eq_cstr(section, "food"))
        return parse_section_food(p, &pack->food);
    if (strview_eq_cstr(section, "audio"))
        return parse_section_audio(p, &pack->audio, path_prefix);
    if (strview_eq_cstr(section, "sprite"))
        return parse_section_sprite(p, &pack->sprites, path_prefix);
    if (strview_eq_cstr(section, "spine"))
        return parse_section_spine(p, &pack->spines, path_prefix);
    if (strview_eq_cstr(section, "snake"))
        return parse_section_snake(p, &pack->snakes);

    return print_error(
        p->filename,
        p->source,
        strspan(section.off, section.len),
        "Unknown section \"%.*s\"\n",
        section.len,
        p->source + section.off);
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
                struct strview section;
                if (scan_next_token(p) != TOK_KEY)
                    return parser_error(
                        p,
                        "Expected a section name within the brackets. "
                        "Example: [section]\n");
                section = p->value.string;
                if (scan_next_token(p) != ']')
                    return parser_error(p, "Missing closing bracket ']'\n");
                tok = parse_section(p, section, pack, path_prefix);
                goto reswitch_tok;
            }

            default:
                return parser_error(
                    p,
                    "Unexpected token encountered. Expected a section name. "
                    "Example: [section]\n");
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
    struct resource_pack* pack;
    struct parser         p;

    pack = mem_alloc(sizeof *pack);
    if (pack == NULL)
        goto alloc_pack_failed;
    resource_pack_init(pack);

    pack->path = pack_path;
    if (str_set_cstr(&pack->pack_ini, pack_path) != 0)
        goto open_pack_ini_failed;
    if (str_join_path_cstr(&pack->pack_ini, "pack.ini") != 0)
        goto open_pack_ini_failed;
    log_info("Reading file \"%s\"\n", str_cstr(pack->pack_ini));
    if (mfile_map_read(&mf, str_cstr(pack->pack_ini), 1))
        goto open_pack_ini_failed;

    parser_init(&p, &mf, str_cstr(pack->pack_ini));
    if (parse_ini(&p, pack, pack_path) != 0)
        goto parse_error;

    if (resource_shader_parse_all(
            str_cstr(pack->pack_ini),
            mf.address,
            mf.size,
            on_section_shader,
            pack) != 0)
        goto parse_error;

    mfile_unmap(&mf);

    return pack;

parse_error:
    mfile_unmap(&mf);
open_pack_ini_failed:
    resource_pack_deinit(pack);
    mem_free(pack);
alloc_pack_failed:
    return NULL;
}

/* ------------------------------------------------------------------------- */
void resource_pack_destroy(struct resource_pack* pack)
{
    resource_pack_deinit(pack);
    mem_free(pack);
}

/* ------------------------------------------------------------------------- */
struct fs_watch* resource_pack_watch_create(struct resource_pack* pack)
{
    struct fs_watch* watch = fs_watch_init();
    if (watch == NULL)
        return NULL;

    fs_watch_file(watch, str_cstr(pack->pack_ini));

    return watch;
}
