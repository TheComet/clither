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

struct gfx_obj_submesh
{
    int    index_count;
    GLuint ibo;
    GLuint program;
    GLuint tex[MAX_TEXTURE_SAMPLERS];
    GLuint sTex[MAX_TEXTURE_SAMPLERS];
    GLuint sMvp;
};

VEC_DECLARE(gfx_obj_submesh_vec, struct gfx_obj_submesh, 8)

struct gfx_obj
{
    GLuint                      vbo;
    struct gfx_obj_submesh_vec* submeshes;
};

void gfx_gles2_obj_init(struct gfx_obj* obj);
void gfx_gles2_obj_deinit(struct gfx_obj* obj);
int  gfx_gles2_obj_load(
     struct gfx_obj*               obj,
     const struct resource_object* res,
     const struct resource_shader* shader);
void gfx_gles2_obj_unload(struct gfx_obj* obj);

void gfx_gles2_obj_draw(const struct gfx_obj* obj);
