#pragma once

#include "./gfx_constants.h"
#include "clither/game/q.h"
#include "glad/gles2.h"

struct aspect_ratio;
struct camera;
struct quad_mesh;
struct resource_shader;
struct resource_layer;

struct sprite_mat
{
    GLuint program;
    GLuint uAspectRatio;
    GLuint uPosCameraSpace;
    GLuint uDir;
    GLuint uSize;
    GLuint uAnim;
    GLuint sTex[MAX_TEXTURE_SAMPLERS];
};

void gfx_gles2_sprite_mat_init(struct sprite_mat* mat);
void gfx_gles2_sprite_mat_deinit(struct sprite_mat* mat);
int  gfx_gles2_sprite_mat_load(
     struct sprite_mat* mat, const struct resource_shader* shader);
void gfx_gles2_sprite_mat_unload(struct sprite_mat* mat);

struct sprite_tex
{
    GLuint  tex[MAX_TEXTURE_SAMPLERS];
    GLfloat scale;
    int8_t  tile_x, tile_y, tile_count, fps, anim_frame;
    float   sim_time;
};

void gfx_gles2_sprite_tex_init(struct sprite_tex* tex);
void gfx_gles2_sprite_tex_deinit(struct sprite_tex* tex);
void gfx_gles2_sprite_tex_load(
    struct sprite_tex* tex, const struct resource_layer* res);
void gfx_gles2_sprite_tex_unload(struct sprite_tex* tex);

void gfx_gles2_sprite_prepare_draw(
    const struct quad_mesh*    mesh,
    const struct sprite_mat*   mat,
    const struct aspect_ratio* ar);
void gfx_gles2_sprite_end_draw(void);
void gfx_gles2_sprite_bind_textures(const struct sprite_tex* tex);
void gfx_gles2_sprite_update_uniforms(
    const struct sprite_mat* mat,
    const struct sprite_tex* tex,
    struct qwpos             pos,
    struct qwpos             dir,
    qw                       scale,
    const struct camera*     camera);
void gfx_gles2_sprite_draw(void);

void gfx_gles2_step_sprite_anim(struct sprite_tex* tex, int sim_tick_rate);
