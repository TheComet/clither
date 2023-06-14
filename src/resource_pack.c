#include "clither/log.h"
#include "clither/resource_pack.h"
#include "clither/utf8.h"

#include "cstructures/memory.h"
#include "cstructures/string.h"
#include "cstructures/vector.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------------- */
static struct resource_sprite*
resource_sprite_create(void)
{
    struct resource_sprite* res = MALLOC(sizeof *res);
    res->texture0 = cstr_dup("");
    res->texture1 = cstr_dup("");
    res->tile_x = 1;
    res->tile_y = 1;
    res->num_frames = 1;
    res->fps = 0;
    res->scale = 1.0;
    return res;
}

/* ------------------------------------------------------------------------- */
static void
resource_sprite_destroy(struct resource_sprite* res)
{
    cstr_free(res->texture0);
    cstr_free(res->texture1);
    FREE(res);
}

/* ------------------------------------------------------------------------- */
static struct resource_snake_part*
resource_snake_part_create(void)
{
    struct resource_snake_part* part = MALLOC(sizeof *part);
    memset(part, 0, sizeof *part);
    return part;
}

/* ------------------------------------------------------------------------- */
static void
resource_snake_part_destroy(struct resource_snake_part* part)
{
    if (part->base)       resource_sprite_destroy(part->base);
    if (part->gather)     resource_sprite_destroy(part->gather);
    if (part->boost)      resource_sprite_destroy(part->boost);
    if (part->turn)       resource_sprite_destroy(part->turn);
    if (part->projectile) resource_sprite_destroy(part->projectile);
    if (part->split)      resource_sprite_destroy(part->split);
    if (part->armor)      resource_sprite_destroy(part->armor);
    FREE(part);
}

/* ------------------------------------------------------------------------- */
struct resource_pack*
resource_pack_load(const char* pack_path)
{
    enum section_name
    {
        SEC_NONE,
        SEC_SHADERS,
        SEC_BACKGROUND,
        SEC_HEAD,
        SEC_BODY,
        SEC_TAIL
    };
    enum subsection_name
    {
        SUB_NONE,

        SUB_GLSL,

        SUB_BASE,
        SUB_GATHER,
        SUB_BOOST,
        SUB_TURN,
        SUB_PROJECTILE,
        SUB_SPLIT,
        SUB_ARMOR
    };
    struct section
    {
        enum section_name section;
        enum subsection_name sub;
        int index;
    };

    FILE* fp;
    struct cs_vector glsl_background, glsl_shadow, glsl_sprite;
    struct cs_vector background_sprites;
    struct cs_vector head_parts, body_parts, tail_parts;
    struct cs_string str, line;
    struct section current = { SEC_NONE, SUB_NONE, 0 };
    struct resource_pack* pack = NULL;

    string_init(&str);
    string_cat(string_cat2(&str, pack_path, "/"), "config.ini");

    fp = utf8_fopen_rb(string_cstr(&str), string_length(&str));
    if (fp == NULL)
    {
        log_err("Failed to open file \"%s\": \"%s\"\n", string_cstr(&str), strerror(errno));
        goto open_config_ini_failed;
    }

    vector_init(&glsl_background, sizeof(char*));
    vector_init(&glsl_shadow, sizeof(char*));
    vector_init(&glsl_sprite, sizeof(char*));

    vector_init(&background_sprites, sizeof(struct resource_sprite*));

    vector_init(&head_parts, sizeof(struct resource_snake_part*));
    vector_init(&body_parts, sizeof(struct resource_snake_part*));
    vector_init(&tail_parts, sizeof(struct resource_snake_part*));

    log_dbg("Reading file \"%s\"\n", string_cstr(&str));
    string_init(&line);
    while (string_getline(&line, fp) > 0)
    {
        char* s = string_cstr(&line);
        char* key;
        char* value;

        /* Parse a [section] in ini file */
        if (s[0] == '[')
        {
            char* index = s + 1;
            char* subsection = s + 1;
            char* section = s + 1;

            /* Extract text inside [..] */
            while (*s && *s != ']')
                ++s;
            if (!*s)
            {
                log_err("Syntax error: Expected to find a matching ']'\n");
                goto parse_error;
            }
            *s = '\0';

            /* Get subsection name [section.subsection] */
            while (*subsection && *subsection != '.')
                ++subsection;
            if (*subsection)
                *subsection++ = '\0';

            /* Get section index [sectionXX.subsection] and convert to integer */
            while (*index && (*index < '0' || *index > '9'))
                ++index;
            if (*index)
            {
                current.index = atoi(index);
                *index = '\0';
                if (current.index > 50 || current.index < 0)
                {
                    log_err("Index %d is larger than reasonable limit of 50. Aborting\n", current.index);
                    goto parse_error;
                }
            }

            if (strcmp(section, "shaders") == 0)
                current.section = SEC_SHADERS;
            else if (strcmp(section, "background") == 0)
                current.section = SEC_BACKGROUND;
            else if (strcmp(section, "head") == 0)
                current.section = SEC_HEAD;
            else if (strcmp(section, "body") == 0)
                current.section = SEC_BODY;
            else if (strcmp(section, "tail") == 0)
                current.section = SEC_TAIL;
            else
            {
                log_warn("Unknown section \"%s\", skipping\n", s);
                current.section = SEC_NONE;
                continue;
            }

            /*
             * As later lines are parsed, they will index into these vectors
             * and modify the members using current.index. Ensure that these
             * vectors have the correct size.
             */
            if (current.section == SEC_BACKGROUND)
                while ((int)vector_count(&background_sprites) < current.index + 1)
                    *(struct resource_sprite**)vector_emplace(&background_sprites) =
                    resource_sprite_create();
            if (current.section == SEC_HEAD)
                while ((int)vector_count(&head_parts) < current.index + 1)
                    *(struct resource_snake_part**)vector_emplace(&head_parts) =
                    resource_snake_part_create();
            if (current.section == SEC_BODY)
                while ((int)vector_count(&body_parts) < current.index + 1)
                    *(struct resource_snake_part**)vector_emplace(&body_parts) =
                    resource_snake_part_create();
            if (current.section == SEC_TAIL)
                while ((int)vector_count(&tail_parts) < current.index + 1)
                    *(struct resource_snake_part**)vector_emplace(&tail_parts) =
                    resource_snake_part_create();

            current.sub = SUB_NONE;
            if (current.section == SEC_SHADERS)
            {
                if (strcmp(subsection, "glsl") == 0)
                    current.sub = SUB_GLSL;
                else
                {
                    if (*subsection)
                        log_warn("Unknown subsection \"%s\", skipping\n", subsection);
                    else
                        log_warn("Missing subsection \"glsl\" in section \"%s\", skipping\n", section);
                    current.section = SEC_NONE;
                    continue;
                }
            }
            else if (current.section == SEC_HEAD || current.section == SEC_BODY || current.section == SEC_TAIL)
            {
                if (strcmp(subsection, "base") == 0)
                    current.sub = SUB_BASE;
                else if (strcmp(subsection, "gather") == 0)
                    current.sub = SUB_GATHER;
                else if (strcmp(subsection, "boost") == 0)
                    current.sub = SUB_BOOST;
                else if (strcmp(subsection, "turn") == 0)
                    current.sub = SUB_TURN;
                else if (strcmp(subsection, "projectile") == 0)
                    current.sub = SUB_PROJECTILE;
                else if (strcmp(subsection, "split") == 0)
                    current.sub = SUB_SPLIT;
                else if (strcmp(subsection, "armor") == 0)
                    current.sub = SUB_ARMOR;
                else
                {
                    if (*subsection)
                        log_warn("Unknown subsection \"%s\", skipping\n", subsection);
                    else
                        log_warn("Missing subsection \"glsl\" in section \"%s\", skipping\n", section);
                    current.section = SEC_NONE;
                    continue;
                }
            }

            /* Section parsed, continue with next line */
            continue;
        }

        /* Keys cannot have spaces */
        key = s;
        while (*s && *s != ' ' && *s != '=')
            ++s;
        if (!*s)
        {
            log_err("Syntax error on line \"%s\": Expected '='\n", string_cstr(&line));
            goto parse_error;
        }
        *s = '\0';
        ++s;

        /* Get rid of joining " = " */
        while (*s && (*s == ' ' || *s == '='))
            ++s;
        if (!*s)
        {
            log_err("Syntax error on line \"%s\": Expected a value\n", string_cstr(&line));
            goto parse_error;
        }
        value = s;

        /* Remove trailing comments */
        while (*s && *s != ';' && *s != '#')
            ++s;
        *s = '\0';

        switch (current.section)
        {
        case SEC_NONE:
            break;

        /*
         * [shaders.glsl]
         * shadow = ...
         * sprite = ...
         * background = ...
         */
        case SEC_SHADERS: {
            switch (current.sub)
            {
            case SUB_GLSL: {
                char* saveptr;
                for (s = string_tok_strip_c(value, ',', ' ', &saveptr); s; s = string_tok_strip_c(NULL, ',', ' ', &saveptr))
                {
                    string_cat(string_cat2(&str, pack_path, "/"), s);
                    if (strcmp(key, "shadow") == 0)
                        *(char**)vector_emplace(&glsl_shadow) = string_take(&str);
                    else if (strcmp(key, "sprite") == 0)
                        *(char**)vector_emplace(&glsl_sprite) = string_take(&str);
                    else if (strcmp(key, "background") == 0)
                        *(char**)vector_emplace(&glsl_background) = string_take(&str);
                    else
                    {
                        log_warn("Unknown key \"%s\"\n", key);
                        break;
                    }
                }
            } break;

            case SUB_NONE:
            case SUB_BASE:
            case SUB_GATHER:
            case SUB_BOOST:
            case SUB_TURN:
            case SUB_PROJECTILE:
            case SUB_SPLIT:
            case SUB_ARMOR:
                log_warn("Unknown subsection\n");
                break;
            }
        } break;

        /*
         * [background]
         * textures = col.png, nor.png
         */
        case SEC_BACKGROUND: {
            if (strcmp(key, "textures") == 0)
            {
                struct resource_sprite* res;
                char* tex1;
                char* tex0 = value;
                cstr_split2_strip(&tex0, &tex1, ',', ' ');
                if (!*tex1)
                    log_warn("Missing normal map in background texture\n");
                res = *(struct resource_sprite**)vector_get_element(&background_sprites, current.index);
                string_cat(string_cat2(&str, pack_path, "/"), tex0);
                FREE(res->texture0); res->texture0 = string_take(&str);
                string_cat(string_cat2(&str, pack_path, "/"), tex1);
                FREE(res->texture1); res->texture1 = string_take(&str);
            }
            else
                log_warn("Unknown key \"%s\"\n", key);
        } break;

        /*
         * [head0.base]
         * textures = col.png, nor.png
         * tile = 4,3
         * frames = 12
         * fps = 20
         * size = 1.0
         */
        case SEC_HEAD:
        case SEC_BODY:
        case SEC_TAIL: {
            struct resource_sprite* sprite;
            struct cs_vector* vec =
                current.section == SEC_HEAD ? &head_parts :
                current.section == SEC_BODY ? &body_parts :
                &tail_parts;
            struct resource_snake_part* part =
                *(struct resource_snake_part**)vector_get_element(vec, current.index);
            struct resource_sprite** psprite =
                current.sub == SUB_BASE ? &part->base :
                current.sub == SUB_GATHER ? &part->gather :
                current.sub == SUB_BOOST ? &part->boost :
                current.sub == SUB_TURN ? &part->turn :
                current.sub == SUB_PROJECTILE ? &part->projectile :
                current.sub == SUB_SPLIT ? &part->split :
                current.sub == SUB_ARMOR ? &part->armor :
                NULL;

            if (psprite == NULL)
            {
                log_warn("Unknown subsection\n");
                break;
            }
            if (*psprite == NULL)
                *psprite = resource_sprite_create();
            sprite = *psprite;

            if (strcmp(key, "textures") == 0)
            {
                char* tex1;
                char* tex0 = value;
                cstr_split2_strip(&tex0, &tex1, ',', ' ');
                if (!*tex1)
                    log_warn("Missing normal map in snake part\n");
                string_cat(string_cat2(&str, pack_path, "/"), tex0);
                FREE(sprite->texture0); sprite->texture0 = string_take(&str);
                string_cat(string_cat2(&str, pack_path, "/"), tex1);
                FREE(sprite->texture1); sprite->texture1 = string_take(&str);
            }
            else if (strcmp(key, "tile") == 0)
            {
                int tile_x, tile_y;
                char* str_y;
                char* str_x = value;
                cstr_split2_strip(&str_x, &str_y, ',', ' ');
                tile_x = atoi(str_x);
                tile_y = atoi(str_y);
                if (tile_x < 1 || tile_x > 64)
                {
                    log_err("Invalid tile X dimension %s\n", str_x);
                    break;
                }
                if (tile_y < 1 || tile_y > 64)
                {
                    log_err("Invalid tile Y dimension %s\n", str_y);
                    break;
                }
                sprite->tile_x = tile_x;
                sprite->tile_y = tile_y;
            }
            else if (strcmp(key, "frames") == 0)
            {
                int frames = atoi(value);
                if (frames < 1 || frames > 64 * 64)
                {
                    log_err("Invalid frame count %s\n", value);
                    break;
                }
                sprite->num_frames = frames;
            }
            else if (strcmp(key, "fps") == 0)
            {
                int fps = atoi(value);
                if (fps < 1 || fps > 60)
                {
                    log_err("Invalid FPS value %s\n", value);
                    break;
                }
            }
            else if (strcmp(key, "scale") == 0)
            {
                float scale = atof(value);
                if (scale <= 0.0 || scale >= 100.0)
                {
                    log_err("Invalid sprite scale %s\n", value);
                    break;
                }
            }
            else
                log_warn("Unknown key \"%s\"\n", key);
        } break;
        }
    }

    /* NULL terminate all lists */
    *(char**)vector_emplace(&glsl_shadow) = NULL;
    *(char**)vector_emplace(&glsl_sprite) = NULL;
    *(char**)vector_emplace(&glsl_background) = NULL;
    *(struct resource_sprite**)vector_emplace(&background_sprites) = NULL;
    *(struct resource_snake_part**)vector_emplace(&head_parts) = NULL;
    *(struct resource_snake_part**)vector_emplace(&body_parts) = NULL;
    *(struct resource_snake_part**)vector_emplace(&tail_parts) = NULL;

    pack = MALLOC(sizeof *pack);
    pack->shaders.glsl.background = vector_take(&glsl_background);
    pack->shaders.glsl.shadow = vector_take(&glsl_shadow);
    pack->shaders.glsl.sprite = vector_take(&glsl_sprite);
    pack->sprites.background = vector_take(&background_sprites);
    pack->sprites.head = vector_take(&head_parts);
    pack->sprites.body = vector_take(&body_parts);
    pack->sprites.tail = vector_take(&tail_parts);

parse_error:
    string_deinit(&line);
    vector_deinit(&tail_parts);
    vector_deinit(&body_parts);
    vector_deinit(&head_parts);
    vector_deinit(&background_sprites);
    vector_deinit(&glsl_sprite);
    vector_deinit(&glsl_shadow);
    vector_deinit(&glsl_background);
    fclose(fp);
open_config_ini_failed:
    string_deinit(&str);
    return pack;
}

/* ------------------------------------------------------------------------- */
void
resource_pack_destroy(struct resource_pack* pack)
{
#define FREE_ARRAY_OF_ARRAYS(arr, free_func) { \
        int i; \
        for (i = 0; arr[i]; ++i) \
            free_func(arr[i]); \
        FREE(arr); \
    }

    FREE_ARRAY_OF_ARRAYS(pack->shaders.glsl.background, FREE);
    FREE_ARRAY_OF_ARRAYS(pack->shaders.glsl.shadow, FREE);
    FREE_ARRAY_OF_ARRAYS(pack->shaders.glsl.sprite, FREE);
    FREE_ARRAY_OF_ARRAYS(pack->sprites.background, resource_sprite_destroy);
    FREE_ARRAY_OF_ARRAYS(pack->sprites.head, resource_snake_part_destroy);
    FREE_ARRAY_OF_ARRAYS(pack->sprites.body, resource_snake_part_destroy);
    FREE_ARRAY_OF_ARRAYS(pack->sprites.tail, resource_snake_part_destroy);
    FREE(pack);
}
