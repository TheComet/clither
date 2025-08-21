#include "clither/audio/audio.h"
#include "clither/game/resource_pack.h"
#include "clither/game/resource_pack_ini.h"
#include "clither/platform/fs.h"
#include "clither/platform/mfile.h"
#include "clither/util/log.h"
#include "clither/util/mem.h"
#include "clither/util/str.h"
#include "clither/util/strlist.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

HMAP_DEFINE_STR(extern, resource_shader_hmap, struct resource_shader, 16)
HMAP_DEFINE_STR(extern, resource_sprite_hmap, struct resource_sprite, 16)
HMAP_DEFINE_STR(extern, resource_snake_hmap, struct resource_snake, 16)
HMAP_DEFINE_STR(extern, resource_spine_hmap, struct resource_spine, 16)

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
static int patch_str_paths(void* value, int type, void* user_ptr)
{
    struct str**                str;
    const struct resource_pack* pack;

    if (type != 3 /* custom str */)
        return 0;

    pack = user_ptr;
    str = value;
    return str_join_path_prepend_cstr(str, pack->path);
}

/* ------------------------------------------------------------------------- */
static int patch_stringlist_paths(void* value, int type, void* user_ptr)
{
    int                         idx;
    const struct resource_pack* pack;
    struct strview              view;
    struct str*                 str;
    struct strlist**            l;

    if (type != 6 /* custom strlist */)
        return 0;

    pack = user_ptr;
    l = value;
    str_init(&str);

    strlist_for_each (*l, idx, view)
    {
        if (str_set_cstr(&str, pack->path) != 0)
            goto fail;
        if (str_join_path(&str, view) != 0)
            goto fail;
        if (strlist_set_view(l, idx, str_view(str)) != 0)
            goto fail;
    }

    str_deinit(str);
    return 0;

fail:
    str_deinit(str);
    return -1;
}

/* ------------------------------------------------------------------------- */
static int on_section_shader(struct c_ini_parser* p, void* user_ptr)
{
    int                     result;
    struct resource_shader  shader;
    struct resource_pack*   pack = user_ptr;
    struct resource_shader* inserted = NULL;

    resource_shader_init(&shader);
    result = resource_shader_parse_section(&shader, p);
    if (result < 0)
        goto failed;

    switch (resource_shader_hmap_emplace_or_get(
        &pack->shaders, str_view(shader.target), &inserted))
    {
        case HMAP_NEW: break;
        case HMAP_EXISTS:
            log_err(
                "Shader target \"%s\" already exists\n",
                str_cstr(shader.target));
        case HMAP_OOM: goto failed;
    }

    if (resource_shader_for_each_value(&shader, patch_stringlist_paths, pack) !=
        0)
        goto failed;

    *inserted = shader;
    return result;

failed:
    resource_shader_deinit(&shader);
    return -1;
}

/* ------------------------------------------------------------------------- */
static int on_section_layer(struct c_ini_parser* p, void* user_ptr)
{
    int                      result;
    struct resource_layer    layer;
    struct resource_pack*    pack = user_ptr;
    struct resource_sprite*  sprite = NULL;
    enum resource_layer_name layer_idx;

    resource_layer_init(&layer);
    result = resource_layer_parse_section(&layer, p);
    if (result < 0)
        goto failed;

    switch (resource_sprite_hmap_emplace_or_get(
        &pack->sprites, str_view(layer.name), &sprite))
    {
        case HMAP_OOM: goto failed;
        case HMAP_NEW: resource_sprite_init(sprite);
        case HMAP_EXISTS: break;
    }

    if (str_eq_cstr(layer.layer, "base"))
        layer_idx = RESOURCE_LAYER_BASE;
    else if (str_eq_cstr(layer.layer, "gather"))
        layer_idx = RESOURCE_LAYER_GATHER;
    else if (str_eq_cstr(layer.layer, "boost"))
        layer_idx = RESOURCE_LAYER_BOOST;
    else if (str_eq_cstr(layer.layer, "turn"))
        layer_idx = RESOURCE_LAYER_TURN;
    else if (str_eq_cstr(layer.layer, "projectile"))
        layer_idx = RESOURCE_LAYER_PROJECTILE;
    else if (str_eq_cstr(layer.layer, "split"))
        layer_idx = RESOURCE_LAYER_SPLIT;
    else if (str_eq_cstr(layer.layer, "armor"))
        layer_idx = RESOURCE_LAYER_ARMOR;
    else
    {
        log_err("Unknown layer \"%s\"\n", str_cstr(layer.name));
        goto failed;
    }

    if (strlist_count(sprite->layer[layer_idx].textures) > 0)
    {
        log_err(
            "Layer \"%s\" was already defined for this sprite\n",
            str_cstr(layer.name));
        goto failed;
    }

    if (resource_layer_for_each_value(&layer, patch_stringlist_paths, pack) !=
        0)
        goto failed;

    sprite->layer[layer_idx] = layer;
    return result;

failed:
    resource_layer_deinit(&layer);
    return -1;
}

/* ------------------------------------------------------------------------- */
static int on_section_spine(struct c_ini_parser* p, void* user_ptr)
{
    int                    result;
    struct resource_spine  spine;
    struct resource_spine* inserted;
    struct resource_pack*  pack = user_ptr;

    resource_spine_init(&spine);
    result = resource_spine_parse_section(&spine, p);
    if (result < 0)
        goto failed;

    switch (resource_spine_hmap_emplace_or_get(
        &pack->spines, str_view(spine.name), &inserted))
    {
        case HMAP_NEW: break;
        case HMAP_EXISTS:
            log_err(
                "Spine with name \"%s\" already exists\n",
                str_cstr(spine.name));
        case HMAP_OOM: goto failed;
    }

    if (resource_spine_for_each_value(&spine, patch_stringlist_paths, pack) !=
        0)
        goto failed;

    *inserted = spine;
    return result;

failed:
    resource_spine_deinit(&spine);
    return -1;
}

/* ------------------------------------------------------------------------- */
static int on_section_snake(struct c_ini_parser* p, void* user_ptr)
{
    int                    result;
    struct resource_snake  snake;
    struct resource_snake* inserted;
    struct resource_pack*  pack = user_ptr;

    resource_snake_init(&snake);
    result = resource_snake_parse_section(&snake, p);
    if (result < 0)
        goto failed;

    switch (resource_snake_hmap_emplace_or_get(
        &pack->snakes, str_view(snake.name), &inserted))
    {
        case HMAP_NEW: break;
        case HMAP_EXISTS:
            log_err(
                "Spine with name \"%s\" already exists\n",
                str_cstr(snake.name));
        case HMAP_OOM: goto failed;
    }

    *inserted = snake;
    return result;

failed:
    resource_snake_deinit(&snake);
    return -1;
}

/* ------------------------------------------------------------------------- */
struct resource_pack* resource_pack_parse(const char* pack_path)
{
    struct mfile          mf;
    struct resource_pack* pack;
    const char*           filename;

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

    filename = str_cstr(pack->pack_ini);

    if (resource_background_parse(
            &pack->background, filename, mf.address, mf.size) != 0 ||
        resource_background_for_each_value(
            &pack->background, patch_stringlist_paths, pack) != 0)
        goto parse_error;

    if (resource_text_parse(&pack->text, filename, mf.address, mf.size) != 0 ||
        resource_text_for_each_value(&pack->text, patch_str_paths, pack) != 0)
        goto parse_error;

    if (resource_food_parse(&pack->food, filename, mf.address, mf.size) != 0 ||
        resource_food_for_each_value(
            &pack->food, patch_stringlist_paths, pack) != 0)
        goto parse_error;

    if (resource_audio_parse(&pack->audio, filename, mf.address, mf.size) !=
            0 ||
        resource_audio_for_each_value(
            &pack->audio, patch_stringlist_paths, pack) != 0)
        goto parse_error;

    if (resource_shader_parse_all(
            filename, mf.address, mf.size, on_section_shader, pack) != 0)
        goto parse_error;

    if (resource_layer_parse_all(
            filename, mf.address, mf.size, on_section_layer, pack) != 0)
        goto parse_error;

    if (resource_spine_parse_all(
            filename, mf.address, mf.size, on_section_spine, pack) != 0)
        goto parse_error;

    if (resource_snake_parse_all(
            filename, mf.address, mf.size, on_section_snake, pack) != 0)
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
