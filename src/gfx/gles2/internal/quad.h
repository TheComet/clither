#pragma once

#include "glad/gles2.h"

struct gfx_tracker;

struct vertex
{
    GLfloat pos[2];
};

struct quad_mesh
{
    GLuint vbo;
};

extern const struct vertex gfx_gles2_quad_vertices[6];
extern const char*         gfx_gles2_quad_attr_bindings[2];

void gfx_gles2_quad_mesh_init(
    struct quad_mesh* mesh, struct gfx_tracker* track);
void gfx_gles2_quad_mesh_deinit(
    struct quad_mesh* mesh, struct gfx_tracker* track);
void gfx_gles2_quad_mesh_prepare_draw(const struct quad_mesh* mesh);
void gfx_gles2_quad_mesh_end_draw(void);
void gfx_gles2_quad_mesh_draw(void);
