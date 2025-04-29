#include "./quad.h"
#include "clither/util/log.h"
#include <stddef.h> /* NULL */

/* clang-format off */
const struct vertex gfx_gles2_quad_vertices[4] = {
    {{-1, -1}, {0, 1}},
    {{-1,  1}, {0, 0}},
    {{ 1, -1}, {1, 1}},
    {{ 1,  1}, {1, 0}}};
const GLushort gfx_gles2_quad_indices[6] = {0, 2, 1, 1, 3, 2};
const char* gfx_gles2_quad_attr_bindings[3] = {
    "vPosition",
    "vTexCoord",
    NULL};
/* clang-format on */

void gfx_gles2_quad_mesh_init(struct quad_mesh* sm)
{
    glGenBuffers(1, &sm->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, sm->vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(gfx_gles2_quad_vertices),
        gfx_gles2_quad_vertices,
        GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &sm->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sm->ibo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        sizeof(gfx_gles2_quad_indices),
        gfx_gles2_quad_indices,
        GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void gfx_gles2_quad_mesh_deinit(struct quad_mesh* sm)
{
    glDeleteBuffers(1, &sm->ibo);
    glDeleteBuffers(1, &sm->vbo);
}
