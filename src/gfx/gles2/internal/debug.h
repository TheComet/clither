#pragma once

#include "clither/game/q.h"
#include "clither/util/vec.h"
#include "glad/gles2.h"

struct aspect_ratio;
struct camera;
struct gfx_quad_mesh;

struct debug_circle
{
    struct qwpos pos;
    qw           radius;
    uint32_t     rgba;
};

VEC_DECLARE(debug_circle_vec, struct debug_circle, 16)

struct debug_mat
{
    GLuint program;
    GLuint uPosCameraSpace;
    GLuint uAspectRatio;
    GLuint uSize;
    GLuint uColor;
};

struct gfx_debug
{
    struct debug_mat         mat;
    struct debug_circle_vec* circles;
};

void gfx_gles2_debug_init(struct gfx_debug* debug);
void gfx_gles2_debug_deinit(struct gfx_debug* debug);
int  gfx_gles2_debug_load(struct gfx_debug* debug);
void gfx_gles2_debug_unload(struct gfx_debug* debug);
void gfx_gles2_debug_draw(
    struct gfx_debug*              debug,
    const struct gfx_quad_mesh*    mesh,
    const struct camera*       camera,
    const struct aspect_ratio* ar);
