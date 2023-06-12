#pragma once

#include "clither/config.h"

C_BEGIN

struct resource_sprite
{
    char* texture0;
    char* texture1;
    int tile_x, tile_y;
    int num_frames;
    int fps;
    float scale;
};

/*!
 * A snake part consists of multiple sprites layered on top of each other.
 * Layers are drawn/not drawn depending on which upgrades the player has
 * activated.
 *
 * Also note that not all parts have all layers. If a layer does not exist,
 * then the sprite's texture name will be an empty string.
 */
struct resource_snake_part
{
    struct resource_sprite base;
    struct resource_sprite gather;
    struct resource_sprite boost;
    struct resource_sprite turn;
    struct resource_sprite projectile;
    struct resource_sprite split;
    struct resource_sprite armor;
};

struct resource_pack
{
    struct {
        struct {
            /*! List of source files that comprise the background shader. List is NULL terminated */
            char** background;
            /*! List of source files that comprise the shadow shader. List is NULL terminated */
            char** shadow;
            /*! List of source files that comprise the sprite shader. List is NULL terminated */
            char** sprite;
        } glsl;
    } shaders;

    struct {
        /*! List of textures that make up the background. There are multiple
         * layers of textures with different grid spacings that make up the
         * background. The details of how they're pieced together is all part
         * of the background shader */
        struct resource_sprite** background;
        /*! List of head parts. List is NULL terminated */
        struct resource_snake_part** head;
        /*! List of body parts. List is NULL terminated */
        struct resource_snake_part** body;
        /*! List of tail parts. List is NULL terminated */
        struct resource_snake_part** tail;
    } sprites;
};

struct resource_pack*
resource_pack_load(const char* pack_path);

void
resource_pack_destroy(struct resource_pack* pack);

C_END
