#pragma once

#include "clither/util/hmap_str.h"

struct str;

/*!
 * An object such as a snake part consists of multiple sprites layered on top
 * of each other. Layers are drawn/not drawn depending on which upgrades the
 * player has activated.
 *
 * Also note that not all parts have all layers. If a layer does not exist,
 * then the sprite's "textures" list will be empty.
 */
enum resource_layer_name
{
    RESOURCE_LAYER_BASE = 0,
    RESOURCE_LAYER_GATHER,
    RESOURCE_LAYER_BOOST,
    RESOURCE_LAYER_TURN,
    RESOURCE_LAYER_PROJECTILE,
    RESOURCE_LAYER_SPLIT,
    RESOURCE_LAYER_ARMOR,

    RESOURCE_LAYER_COUNT
};

struct resource_shader
{
    /*! List of source files that comprise the background shader. */
    struct strlist* background;
    /*! List of source files that comprise the shadow shader. */
    struct strlist* shadow;
    /*! List of source files that comprise the sprite shader. */
    struct strlist* sprite;
    /*! List of source files that comprise the text shader. */
    struct strlist* text;
    /*! List of source files that comprise the spine shader. */
    struct strlist* spine;
};

struct resource_background
{
    struct strlist* textures;
};

struct resource_text
{
    struct str* font; /*! Font face filename to use for text rendering */
    int         size;
    int         dpi;
};

struct resource_layer
{
    struct strlist* textures;
    int             tile_x, tile_y;
    int             num_frames;
    int             fps;
};

struct resource_sprite
{
    struct resource_layer layer[RESOURCE_LAYER_COUNT];
};
struct resource_spine
{
    struct strlist* textures;
};

struct resource_food
{
    struct str* sprite;
    float       scale;
};

struct resource_snake
{
    struct str*     head_sprite;
    struct str*     tail_sprite;
    struct strlist* body_sprites;
    struct str*     spine;
};

HMAP_DECLARE_STR(extern, resource_shader_hmap, struct resource_shader, 16)
HMAP_DECLARE_STR(extern, resource_sprite_hmap, struct resource_sprite, 16)
HMAP_DECLARE_STR(extern, resource_snake_hmap, struct resource_snake, 16)
HMAP_DECLARE_STR(extern, resource_spine_hmap, struct resource_spine, 16)

struct resource_pack
{
    const char* path;
    struct str* pack_ini;

    struct resource_background background;
    struct resource_text       text;
    struct resource_food       food;

    struct resource_spine_hmap*  spines;
    struct resource_shader_hmap* shaders;
    struct resource_sprite_hmap* sprites;
    struct resource_snake_hmap*  snakes;
};

struct resource_pack* resource_pack_parse(const char* pack_path);
void                  resource_pack_destroy(struct resource_pack* pack);

struct fs_watch* resource_pack_watch_create(struct resource_pack* pack);
