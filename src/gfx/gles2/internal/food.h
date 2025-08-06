#pragma once

#include "./sprite.h"

struct aspect_ratio;
struct camera;
struct food_bmap;
struct gfx;
struct resource_pack;

struct gfx_food
{
    struct gfx_sprite_tex tex;
    GLfloat               scale;
};

void gfx_gles2_food_init(struct gfx_food* gfx);
void gfx_gles2_food_deinit(struct gfx_food* gfx);
void gfx_gles2_food_load(
    struct gfx_food* gfx, const struct resource_pack* pack);
void gfx_gles2_food_unload(struct gfx_food* gfx);

void gfx_gles2_food_step_anim(struct gfx_food* gfx, int sim_tick_rate);

void gfx_gles2_draw_food_shadows(
    const struct food_bmap*    food_bmap,
    const struct gfx*          gfx,
    const struct gfx_food*     food,
    const struct camera*       camera,
    const struct aspect_ratio* ar,
    int                        shadow_map_size_factor);

void gfx_gles2_draw_food(
    const struct food_bmap*    food_bmap,
    const struct gfx*          gfx,
    const struct gfx_food*     food,
    const struct camera*       camera,
    const struct aspect_ratio* ar);
