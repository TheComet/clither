#pragma once

#include "clither/game/q.h"
#include "glad/gles2.h"

struct aspect_ratio;
struct background;
struct camera;
struct resource_pack;
struct sprite_tex;
struct quad_mesh;

struct sprite_shadow_mat
{
    GLuint program;
    GLuint uAspectRatio;
    GLuint uPosCameraSpace;
    GLuint uDir;
    GLuint uSize;
    GLuint uAnim;
    GLuint sNM;
};

void gfx_gles2_sprite_shadow_init(struct sprite_shadow_mat* ss);
void gfx_gles2_sprite_shadow_deinit(struct sprite_shadow_mat* ss);
int  gfx_gles2_sprite_shadow_load(
     struct sprite_shadow_mat* ss, const struct resource_pack* pack);
void gfx_gles2_sprite_shadow_unload(struct sprite_shadow_mat* ss);

void gfx_gles2_sprite_shadow_prepare_draw(
    const struct background*        bg,
    const struct quad_mesh*         mesh,
    const struct sprite_shadow_mat* mat,
    const struct aspect_ratio*      ar,
    GLint                           gfx_width,
    GLint                           gfx_height,
    int                             shadow_map_size_factor);
void gfx_gles2_sprite_shadow_end_draw(GLint gfx_width, GLint gfx_height);
void gfx_gles2_sprite_shadow_bind_textures(const struct sprite_tex* tex);
void gfx_gles2_sprite_shadow_update_uniforms(
    const struct sprite_shadow_mat* mat,
    const struct sprite_tex*        tex,
    struct qwpos                    pos,
    struct qwpos                    dir,
    qw                              scale,
    const struct camera*            camera);
void gfx_gles2_sprite_shadow_draw(void);
