#pragma once

struct aspect_ratio;
struct camera;
struct food_bmap;
struct gfx;

void gfx_gles2_draw_food_shadows(
    const struct food_bmap*    food_bmap,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar,
    int                        shadow_map_size_factor);

void gfx_gles2_draw_food(
    const struct food_bmap*    food_bmap,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar);
