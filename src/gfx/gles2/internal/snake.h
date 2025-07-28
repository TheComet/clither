#pragma once

#include "./spine.h"
#include "./sprite.h"
#include "clither/util/vec.h"

struct gfx_part_sample
{
    struct qwpos pos;
    struct qwpos dir;
    qw           length;
};

VEC_DECLARE(gfx_part_sample_vec, struct gfx_part_sample, 32)
VEC_DECLARE(gfx_sprite_tex_vec, struct gfx_sprite_tex, 8)

struct aspect_ratio;
struct camera;
struct gfx;
struct resource_pack;
struct resource_shader;
struct resource_snake;
struct snake;

struct gfx_snake
{
    struct gfx_sprite_tex      head_base;
    struct gfx_sprite_tex      head_gather;
    struct gfx_sprite_tex_vec* body_base;
    struct gfx_sprite_tex      tail_base;
    struct spine               spine;
    GLfloat                    part_spacing;

    /* Working buffer of samples for rendering */
    struct gfx_part_sample_vec* part_samples;
};

int  gfx_gles2_snake_init(struct gfx_snake* gfx);
void gfx_gles2_snake_deinit(struct gfx_snake* gfx);

int gfx_gles2_snake_load(
    struct gfx_snake*             gfx,
    const struct resource_snake*  res,
    const struct resource_pack*   pack,
    const struct resource_shader* shader);
void gfx_gles2_snake_unload(struct gfx_snake* snake);

void gfx_gles2_snake_step_anim(struct gfx_snake* gfx, int sim_tick_rate);

void gfx_gles2_draw_snake_shadow(
    struct gfx_snake*          gfx_snake,
    const struct gfx*          gfx,
    const struct snake*        snake,
    const struct camera*       camera,
    const struct aspect_ratio* ar,
    int                        shadow_map_size_factor);
void gfx_gles2_draw_snake(
    struct gfx_snake*          gfx_snake,
    const struct gfx*          gfx,
    const struct snake*        snake,
    const struct camera*       camera,
    const struct aspect_ratio* ar);
