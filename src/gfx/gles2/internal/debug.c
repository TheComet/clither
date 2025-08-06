#include "./gfx.h"
#include "./shader.h"
#include "clither/game/camera.h"

VEC_DEFINE(debug_circle_vec, struct debug_circle, 16)

static const char vs[] =
    "#version 330\n"
    "precision mediump float;\n"
    "attribute vec2 vPosition;\n"
    "uniform vec2 uAspectRatio;\n"
    "uniform vec3 uPosCameraSpace;\n"
    "uniform float uSize;\n"
    "varying vec2 fTexCoord;\n"
    "void main()\n"
    "{\n"
    "    fTexCoord = vPosition;\n"
    "    vec2 pos = vPosition * uSize * uPosCameraSpace.z;\n"
    "    pos += uPosCameraSpace.xy;\n"
    "    pos /= uAspectRatio;\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "}\n";
static const char fs[] =
    "#version 330\n"
    "precision mediump float;\n"
    "uniform vec3 uColor;\n"
    "uniform float uSize;\n"
    "varying vec2 fTexCoord;\n"
    "void main()\n"
    "{\n"
    "    float thick = 0.01 / uSize;\n"
    "    float d = sqrt(dot(fTexCoord, fTexCoord));\n"
    "    float t = 1.0 - smoothstep(0.0, thick, abs(1.0 - thick - d));\n"
    "    gl_FragColor = vec4(uColor, t);\n"
    "}\n";

void gfx_gles2_debug_init(struct gfx_debug* debug)
{
    debug->mat.program = 0;
    debug->mat.uPosCameraSpace = (GLuint)-1;
    debug->mat.uAspectRatio = (GLuint)-1;
    debug->mat.uSize = (GLuint)-1;
    debug->mat.uColor = (GLuint)-1;

    debug_circle_vec_init(&debug->circles);
}

void gfx_gles2_debug_deinit(struct gfx_debug* debug)
{
    debug_circle_vec_deinit(debug->circles);

    if (debug->mat.program != 0)
        glDeleteProgram(debug->mat.program);
}

int gfx_gles2_debug_load(struct gfx_debug* debug)
{
    static const char* attr_bindings[] = {"vPosition", NULL};
    debug->mat.program = gfx_gles2_load_shader_str(vs, fs, attr_bindings);
    if (debug->mat.program == INVALID_HANDLE)
        return -1;

    debug->mat.uAspectRatio = gfx_gles2_get_uniform_location_and_warn(
        debug->mat.program, "uAspectRatio");
    debug->mat.uPosCameraSpace = gfx_gles2_get_uniform_location_and_warn(
        debug->mat.program, "uPosCameraSpace");
    debug->mat.uSize =
        gfx_gles2_get_uniform_location_and_warn(debug->mat.program, "uSize");
    debug->mat.uColor =
        gfx_gles2_get_uniform_location_and_warn(debug->mat.program, "uColor");

    return 0;
}

void gfx_gles2_debug_unload(struct gfx_debug* debug)
{
    if (debug->mat.program != INVALID_HANDLE)
    {
        gfx_untrack_shader(debug->mat.program);
        glDeleteProgram(debug->mat.program);
    }
    debug->mat.program = INVALID_HANDLE;
}

static void draw_circle(
    const struct debug_mat*    mat,
    const struct debug_circle* circle,
    const struct camera*       camera)
{
    struct qwpos pos_cameraSpace;
    pos_cameraSpace.x =
        qw_mul(qw_sub(circle->pos.x, camera->pos.x), camera->scale);
    pos_cameraSpace.y =
        qw_mul(qw_sub(circle->pos.y, camera->pos.y), camera->scale);

    glUniform1f(mat->uSize, qw_to_float(circle->radius));
    glUniform3f(
        mat->uPosCameraSpace,
        qw_to_float(pos_cameraSpace.x),
        qw_to_float(pos_cameraSpace.y),
        qw_to_float(camera->scale));
    glUniform3f(
        mat->uColor,
        (circle->rgba >> 24) / 255.0f,
        ((circle->rgba >> 16) & 0xFF) / 255.0f,
        ((circle->rgba >> 8) & 0xFF) / 255.0f);

    gfx_gles2_quad_mesh_draw();
}

void gfx_gles2_debug_draw(
    struct gfx_debug*           debug,
    const struct gfx_quad_mesh* mesh,
    const struct camera*        camera,
    const struct aspect_ratio*  ar)
{
    const struct debug_circle* circle;

    gfx_gles2_quad_mesh_prepare_draw(mesh);
    glUseProgram(debug->mat.program);
    glUniform2f(debug->mat.uAspectRatio, ar->scale_x, ar->scale_y);

    vec_for_each (debug->circles, circle)
        draw_circle(&debug->mat, circle, camera);

    glUseProgram(0);
    gfx_gles2_quad_mesh_end_draw();

    debug_circle_vec_clear(debug->circles);
}
