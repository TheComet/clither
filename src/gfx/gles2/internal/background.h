#pragma once

#include "./gfx_constants.h"
#include "glad/gles2.h"

struct aspect_ratio;
struct camera;
struct gfx;
struct resource_background;
struct resource_shader;
struct world;

struct gfx_background
{
    GLuint program;
    GLuint fbo;
    GLuint texShadow;
    GLuint tex[MAX_TEXTURE_SAMPLERS];
    GLuint uAspectRatio;
    GLuint uCamera;
    GLuint uShadowInvRes;
    GLuint uWorldBorder;
    GLuint sShadow;
    GLuint sTex[MAX_TEXTURE_SAMPLERS];
};

int gfx_gles2_background_init(
    struct gfx_background* bg,
    int                    fbwidth,
    int                    fbheight,
    int                    shadow_map_size_factor);
void gfx_gles2_background_deinit(struct gfx_background* bg);
void gfx_gles2_background_resize(
    struct gfx_background* bg,
    int                    fbwidth,
    int                    fbheight,
    int                    shadow_map_size_factor);
int gfx_gles2_background_load(
    struct gfx_background*            bg,
    const struct resource_background* res,
    const struct resource_shader*     shader);
void gfx_gles2_background_unload(struct gfx_background* bg);
void gfx_gles2_background_draw(
    const struct world*        world,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar,
    int                        shadow_map_size_factor);
