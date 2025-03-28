#pragma once

struct aspect_ratio;
struct camera;
struct food_grid;
struct gfx;

void gfx_gles2_draw_food_shadows(
    const struct food_grid*    food_grid,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar,
    int                        shadow_map_size_factor);

void gfx_gles2_draw_food(
    const struct food_grid*    food_grid,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar);
