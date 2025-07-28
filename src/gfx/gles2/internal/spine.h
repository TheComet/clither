#pragma once

#include "./gfx_constants.h"
#include "clither/game/q.h"
#include "glad/gles2.h"

struct aspect_ratio;
struct bezier_segment_rb;
struct camera;
struct resource_shader;
struct resource_spine;

struct spine
{
    GLuint vbo;
    GLuint program;

    GLuint uCoeff;
    GLuint uBezierSize;
    GLuint uAspectRatio;
    GLuint uHeadPosition;
    GLuint uScrollScaleOffset;
    GLuint uCutoff;

    GLuint tex[MAX_TEXTURE_SAMPLERS];
    GLuint sTex[MAX_TEXTURE_SAMPLERS];

    GLfloat spine_width;
    GLfloat tex_aspect_ratio;
};

void gfx_gles2_spine_init(struct spine* spine);
void gfx_gles2_spine_deinit(struct spine* spine);
int  gfx_gles2_spine_load(
     struct spine*                 spine,
     const struct resource_spine*  res,
     const struct resource_shader* shader);
void gfx_gles2_spine_unload(struct spine* spine);

void gfx_gles2_spine_prepare_draw(const struct spine* spine);
void gfx_gles2_spine_draw(
    const struct spine*             spine,
    const struct bezier_segment_rb* segments,
    qw                              snake_scale,
    qw                              snake_length,
    const struct camera*            camera,
    const struct aspect_ratio*      ar);
void gfx_gles2_spine_end_draw(void);
