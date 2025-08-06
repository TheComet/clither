#include "./gfx.h"
#include "./gfx_constants.h"
#include "./quad.h"
#include "./rectangle.h"
#include "./shader.h"

static const char* vs =
    "#version 330\n"
    "precision mediump float;\n"
    "attribute vec2 vPosition;\n"
    "uniform vec2 uAspectRatio;\n"
    "uniform vec2 uPosCameraSpace;\n"
    "uniform vec2 uSize;\n"
    "void main()\n"
    "{\n"
    "    vec2 pos = vPosition * uSize;\n"
    "    pos += uPosCameraSpace;\n"
    "    pos /= uAspectRatio;\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "}\n";
static const char* fs =
    "#version 330\n"
    "precision mediump float;\n"
    "uniform vec4 uColor;\n"
    "void main()\n"
    "{\n"
    "    gl_FragColor = uColor;\n"
    "}\n";

/* ------------------------------------------------------------------------- */
int gfx_gles2_rectangle_init(struct gfx_rectangle_mat* rect)
{
    rect->program =
        gfx_gles2_load_shader_str(vs, fs, gfx_gles2_quad_attr_bindings);
    if (rect->program == INVALID_HANDLE)
        return -1;

    rect->uAspectRatio =
        gfx_gles2_get_uniform_location_and_warn(rect->program, "uAspectRatio");
    rect->uPosCameraSpace = gfx_gles2_get_uniform_location_and_warn(
        rect->program, "uPosCameraSpace");
    rect->uSize =
        gfx_gles2_get_uniform_location_and_warn(rect->program, "uSize");
    rect->uColor =
        gfx_gles2_get_uniform_location_and_warn(rect->program, "uColor");

    return 0;
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_rectangle_deinit(struct gfx_rectangle_mat* rect)
{
    if (rect->program != INVALID_HANDLE)
    {
        gfx_untrack_shader(rect->program);
        glDeleteProgram(rect->program);
    }
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_rectangle_draw(
    const struct gfx_rectangle_mat* rect,
    const struct gfx_quad_mesh*     quad_mesh,
    struct fpos                     pos,
    struct fpos                     size,
    uint32_t                        color,
    const struct aspect_ratio*      ar)
{
    gfx_gles2_quad_mesh_prepare_draw(quad_mesh);
    glUseProgram(rect->program);
    glUniform2f(rect->uAspectRatio, ar->scale_x, ar->scale_y);
    glUniform2f(rect->uPosCameraSpace, pos.x, pos.y);
    glUniform2f(rect->uSize, size.x, size.y);
    glUniform4f(
        rect->uColor,
        ((color >> 16) & 0xFF) / 255.0f,
        ((color >> 8) & 0xFF) / 255.0f,
        ((color >> 0) & 0xFF) / 255.0f,
        ((color >> 24) & 0xFF) / 255.0f);
    gfx_gles2_quad_mesh_draw();
    glUseProgram(0);
    gfx_gles2_quad_mesh_end_draw();
}
