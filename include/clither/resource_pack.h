#pragma once

#include "clither/vec.h"

struct str;

struct resource_sprite
{
    struct strlist* textures;
    int             tile_x, tile_y;
    int             num_frames;
    int             fps;
    float           scale;
};

/*!
 * A snake part consists of multiple sprites layered on top of each other.
 * Layers are drawn/not drawn depending on which upgrades the player has
 * activated.
 *
 * Also note that not all parts have all layers. If a layer does not exist,
 * then it will be a NULL pointer.
 */
struct resource_snake_part
{
    struct resource_sprite* base;
    struct resource_sprite* gather;
    struct resource_sprite* boost;
    struct resource_sprite* turn;
    struct resource_sprite* projectile;
    struct resource_sprite* split;
    struct resource_sprite* armor;
};

struct resource_pack
{
    struct
    {
        struct
        {
            /*! List of source files that comprise the background shader. List
             * is NULL terminated */
            struct strlist* background;
            /*! List of source files that comprise the shadow shader. List is
             * NULL terminated */
            struct strlist* shadow;
            /*! List of source files that comprise the sprite shader. List is
             * NULL terminated */
            struct strlist* sprite;
        } glsl;
    } shaders;

    struct
    {
        /*! List of textures that make up the background. There can be multiple
         * layers of textures with different grid spacings. The details of how
         * they're pieced together is all part of the background shader */
        struct resource_sprite_vec* background;
        /*! List of head parts. */
        struct resource_snake_part_vec* heads;
        /*! List of body parts. */
        struct resource_snake_part_vec* bodies;
        /*! List of tail parts. */
        struct resource_snake_part_vec* tails;
    } sprites;
};

struct resource_pack* resource_pack_parse(const char* pack_path);
void                  resource_pack_destroy(struct resource_pack* pack);
