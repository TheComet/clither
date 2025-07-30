#pragma once

#include "clither/game/fpos.h"
#include "glad/gles2.h"

struct aspect_ratio;
struct gfx_quad_mesh;

struct gfx_rectangle_mat
{
    GLuint program;
    GLuint uAspectRatio;
    GLuint uPosCameraSpace;
    GLuint uSize;
    GLuint uColor;
};

int  gfx_gles2_rectangle_init(struct gfx_rectangle_mat* rect);
void gfx_gles2_rectangle_deinit(struct gfx_rectangle_mat* rect);
void gfx_gles2_rectangle_draw(
    const struct gfx_rectangle_mat* rect,
    const struct gfx_quad_mesh*     quad_mesh,
    struct fpos                     pos,
    struct fpos                     size,
    uint32_t                        color,
    const struct aspect_ratio*      ar);
