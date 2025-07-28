#include "./gfx.h"
#include "./quad.h"
#include "clither/util/log.h"
#include <stddef.h> /* NULL */

/* clang-format off */
const struct quad_vertex gfx_gles2_quad_vertices[6] = {
    {{-1, -1}},
    {{-1,  1}},
    {{ 1,  1}},
    {{-1, -1}},
    {{ 1,  1}},
    {{ 1, -1}}
};
const char* gfx_gles2_quad_attr_bindings[2] = {
    "vPosition",
    NULL};
/* clang-format on */

void gfx_gles2_quad_mesh_init(struct gfx_quad_mesh* mesh)
{
    glGenBuffers(1, &mesh->vbo);
    gfx_track_buf(mesh->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(gfx_gles2_quad_vertices),
        gfx_gles2_quad_vertices,
        GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void gfx_gles2_quad_mesh_deinit(struct gfx_quad_mesh* mesh)
{
    gfx_untrack_buf(mesh->vbo);
    glDeleteBuffers(1, &mesh->vbo);
}

void gfx_gles2_quad_mesh_prepare_draw(const struct gfx_quad_mesh* mesh)
{
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(struct quad_vertex),
        (void*)offsetof(struct quad_vertex, pos));
}

void gfx_gles2_quad_mesh_end_draw(void)
{
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void gfx_gles2_quad_mesh_draw(void)
{
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
