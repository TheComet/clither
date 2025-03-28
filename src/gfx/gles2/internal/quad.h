#pragma once

#include "glad/gles2.h"

struct vertex
{
    GLfloat pos[2];
    GLfloat uv[2];
};

struct quad_mesh
{
    GLuint vbo;
    GLuint ibo;
};

extern const struct vertex gfx_gles2_quad_vertices[4];
extern const GLushort      gfx_gles2_quad_indices[6];
extern const char*         gfx_gles2_quad_attr_bindings[3];

void gfx_gles2_quad_mesh_init(struct quad_mesh* sm);
void gfx_gles2_quad_mesh_deinit(struct quad_mesh* sm);
