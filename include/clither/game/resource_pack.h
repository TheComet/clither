#pragma once

#include "clither/game/resource_pack_ini.h"
#include "clither/util/hmap_str.h"

struct str;
struct strlist;

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

SECTION("shader")
struct resource_shader
{
    struct str* target STRING(str);
    /*! List of source files that comprise the background shader. */
    struct strlist* background STRINGLIST(strlist);
    /*! List of source files that comprise the shadow shader. */
    struct strlist* shadow STRINGLIST(strlist);
    /*! List of source files that comprise the sprite shader. */
    struct strlist* sprite STRINGLIST(strlist);
    /*! List of source files that comprise the text shader. */
    struct strlist* text STRINGLIST(strlist);
    /*! List of source files that comprise the spine shader. */
    struct strlist* spine STRINGLIST(strlist);
    /*! List of source files that comprise the wavefront obj shader. */
    struct strlist* obj STRINGLIST(strlist);
};

SECTION("object")
struct resource_object
{
    struct str* name STRING(str);
    struct str* obj  STRING(str);
};

SECTION("background")
struct resource_background
{
    struct strlist* textures STRINGLIST(strlist);
};

SECTION("text")
struct resource_text
{
    /*! Font face filename to use for text rendering */
    struct str* font STRING(str);
    int size         DEFAULT(72);
    int dpi          DEFAULT(72);
};

SECTION("sprite")
struct resource_layer
{
    struct str* name  STRING(str);
    struct str* layer STRING(str);

    struct strlist* textures STRINGLIST(strlist);

    int tile_x     DEFAULT(1);
    int tile_y     DEFAULT(1);
    int num_frames DEFAULT(1);
    int            fps;
};

struct resource_sprite
{
    struct resource_layer layer[RESOURCE_LAYER_COUNT];
};

SECTION("spine")
struct resource_spine
{
    struct str* name         STRING(str);
    struct strlist* textures STRINGLIST(strlist);
};

SECTION("food")
struct resource_food
{
    struct str* sprite STRING(str);
    float scale        DEFAULT(1.0);
};

SECTION("audio")
struct resource_audio
{
    struct str* menu_music STRING(str);

    struct str* button_hover STRING(str);
    struct str* button_click STRING(str);
    struct str* button_back  STRING(str);

    struct str* slider_click   STRING(str);
    struct str* slider_drag    STRING(str);
    struct str* slider_release STRING(str);

    struct str* textinput_type   STRING(str);
    struct str* textinput_delete STRING(str);

    struct str* eat_food STRING(str);
};

SECTION("snake")
struct resource_snake
{
    struct str* name             STRING(str);
    struct str* head_sprite      STRING(str);
    struct str* tail_sprite      STRING(str);
    struct strlist* body_sprites STRINGLIST(strlist);
    struct str* spine            STRING(str);
};

HMAP_DECLARE_STR(extern, resource_shader_hmap, struct resource_shader, 16)
HMAP_DECLARE_STR(extern, resource_sprite_hmap, struct resource_sprite, 16)
HMAP_DECLARE_STR(extern, resource_snake_hmap, struct resource_snake, 16)
HMAP_DECLARE_STR(extern, resource_spine_hmap, struct resource_spine, 16)
HMAP_DECLARE_STR(extern, resource_object_hmap, struct resource_object, 16)

struct resource_pack
{
    const char* path;
    struct str* pack_ini;

    struct resource_background background;
    struct resource_text       text;
    struct resource_food       food;
    struct resource_audio      audio;

    struct resource_spine_hmap*  spines;
    struct resource_shader_hmap* shaders;
    struct resource_sprite_hmap* sprites;
    struct resource_snake_hmap*  snakes;
    struct resource_object_hmap* objects;
};

struct resource_pack* resource_pack_parse(const char* pack_path);
void                  resource_pack_destroy(struct resource_pack* pack);

struct fs_watch* resource_pack_watch_create(struct resource_pack* pack);
