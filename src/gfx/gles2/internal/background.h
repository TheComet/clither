#pragma once

#include "glad/gles2.h"

struct aspect_ratio;
struct camera;
struct gfx;
struct resource_pack;
struct world;

struct background
{
    GLuint program;
    GLuint fbo;
    GLuint texShadow;
    GLuint texCol;
    GLuint texNor;
    GLuint uAspectRatio;
    GLuint uCamera;
    GLuint uShadowInvRes;
    GLuint uWorldBorder;
    GLuint sShadow;
    GLuint sCol;
    GLuint sNM;
};

int gfx_gles2_background_init(
    struct background* bg,
    int                fbwidth,
    int                fbheight,
    int                shadow_map_size_factor);
void gfx_gles2_background_deinit(struct background* bg);

int gfx_gles2_background_load(
    struct background* bg, const struct resource_pack* pack);
void gfx_gles2_background_unload(struct background* bg);

void gfx_gles2_draw_background(
    const struct world*        world,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar,
    int                        shadow_map_size_factor);
