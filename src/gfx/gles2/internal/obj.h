#pragma once

#include "./gfx_constants.h"
#include "clither/util/vec.h"
#include "glad/gles2.h"

struct resource_object;
struct resource_shader;

struct gfx_obj_vertex
{
    GLfloat pos[3];
    GLfloat uv[2];
};

VEC_DECLARE(gfx_obj_vertex_vec, struct gfx_obj_vertex, 32)

struct gfx_obj_mesh
{
    GLuint vbo;
};

struct gfx_obj_tex
{
    GLuint tex[MAX_TEXTURE_SAMPLERS];
};

struct gfx_obj_mat
{
    GLuint ibo;
    GLuint program;
    GLuint sTex[MAX_TEXTURE_SAMPLERS];
};

VEC_DECLARE(gfx_obj_tex_vec, struct gfx_obj_tex, 8)
VEC_DECLARE(gfx_obj_mat_vec, struct gfx_obj_mat, 8)

struct gfx_obj
{
    struct gfx_obj_mesh     mesh;
    struct gfx_obj_tex_vec* tex;
    struct gfx_obj_mat_vec* mat;
};

void gfx_gles2_obj_init(struct gfx_obj* obj);
void gfx_gles2_obj_deinit(struct gfx_obj* obj);
int  gfx_gles2_obj_load(
     struct gfx_obj*               obj,
     const struct resource_object* res,
     const struct resource_shader* shader);
void gfx_gles2_obj_unload(struct gfx_obj* obj);

void gfx_gles2_obj_draw(const struct gfx_obj* obj);
