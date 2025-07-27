#pragma once

struct aspect_ratio;
struct camera;
struct gfx;
struct snake;

void gfx_gles2_draw_snake_shadow(
    const struct snake*        snake,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar,
    int                        shadow_map_size_factor);

void gfx_gles2_draw_snake_spine(
    const struct snake*        snake,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar);

void gfx_gles2_draw_snake(
    const struct snake*        snake,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar);
