#pragma once

#include "./gfx_constants.h"
#include "clither/game/q.h"
#include "glad/gles2.h"

struct aspect_ratio;
struct gfx_background;
struct camera;
struct resource_shader;
struct gfx_sprite_tex;
struct gfx_quad_mesh;

struct gfx_sprite_shadow_mat
{
    GLuint program;
    GLuint uAspectRatio;
    GLuint uPosCameraSpace;
    GLuint uDir;
    GLuint uSize;
    GLuint uAnim;
    GLuint sTex[MAX_TEXTURE_SAMPLERS];
};

void gfx_gles2_sprite_shadow_init(struct gfx_sprite_shadow_mat* ss);
void gfx_gles2_sprite_shadow_deinit(struct gfx_sprite_shadow_mat* ss);
int  gfx_gles2_sprite_shadow_load(
     struct gfx_sprite_shadow_mat* ss, const struct resource_shader* res);
void gfx_gles2_sprite_shadow_unload(struct gfx_sprite_shadow_mat* ss);

void gfx_gles2_sprite_shadow_prepare_draw(
    const struct gfx_background*        bg,
    const struct gfx_quad_mesh*         mesh,
    const struct gfx_sprite_shadow_mat* mat,
    const struct aspect_ratio*          ar,
    GLint                               gfx_width,
    GLint                               gfx_height,
    int                                 shadow_map_size_factor);
void gfx_gles2_sprite_shadow_end_draw(GLint gfx_width, GLint gfx_height);
void gfx_gles2_sprite_shadow_bind_textures(const struct gfx_sprite_tex* tex);
void gfx_gles2_sprite_shadow_update_uniforms(
    const struct gfx_sprite_shadow_mat* mat,
    const struct gfx_sprite_tex*        tex,
    struct qwpos                        pos,
    struct qwpos                        dir,
    GLfloat                             scale,
    const struct camera*                camera);
void gfx_gles2_sprite_shadow_draw(void);
