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
    uint32_t     argb;
};

struct debug_rectangle
{
    struct qwpos top_left, bottom_right;
    uint32_t     argb;
};

struct debug_line
{
    struct qwpos start, end;
    uint32_t     argb;
};

VEC_DECLARE(debug_circle_vec, struct debug_circle, 16)
VEC_DECLARE(debug_rectangle_vec, struct debug_rectangle, 16)
VEC_DECLARE(debug_line_vec, struct debug_line, 16)

struct gfx_debug
{
    struct debug_circle_vec*    circles;
    struct debug_rectangle_vec* rectangles;
    struct debug_line_vec*      lines;
    struct strlist*             strings;

    struct
    {
        GLuint program;
        GLuint uPosCameraSpace;
        GLuint uAspectRatio;
        GLuint uRadius;
        GLuint uThick;
        GLuint uColor;
    } circle_mat;

    struct
    {
        GLuint program;
        GLuint uAspectRatio;
        GLuint uCenterPos;
        GLuint uSize;
        GLuint uAngle;
        GLuint uColor;
    } line_mat;
};

void gfx_gles2_debug_init(struct gfx_debug* debug);
void gfx_gles2_debug_deinit(struct gfx_debug* debug);
int  gfx_gles2_debug_load(struct gfx_debug* debug);
void gfx_gles2_debug_unload(struct gfx_debug* debug);
void gfx_gles2_debug_draw(
    struct gfx*                 gfx,
    struct gfx_debug*           debug,
    const struct gfx_quad_mesh* mesh,
    const struct camera*        camera,
    const struct aspect_ratio*  ar);
