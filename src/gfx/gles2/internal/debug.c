#include "./gfx.h"
#include "./shader.h"
#include "clither/game/camera.h"
#include "clither/util/strlist.h"

VEC_DEFINE(debug_circle_vec, struct debug_circle, 16)
VEC_DEFINE(debug_rectangle_vec, struct debug_rectangle, 16)
VEC_DEFINE(debug_line_vec, struct debug_line, 16)

static const char circle_vs[] =
    "precision mediump float;\n"
    "attribute vec2 vPosition;\n"
    "uniform vec2 uAspectRatio;\n"
    "uniform vec3 uPosCameraSpace;\n"
    "uniform float uRadius;\n"
    "varying vec2 fTexCoord;\n"
    "void main()\n"
    "{\n"
    "    fTexCoord = vPosition;\n"
    "    vec2 pos = vPosition * uRadius * uPosCameraSpace.z;\n"
    "    pos += uPosCameraSpace.xy;\n"
    "    pos /= uAspectRatio;\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "}\n";
static const char circle_fs[] =
    "precision mediump float;\n"
    "uniform vec4 uColor;\n"
    "uniform float uRadius;\n"
    "uniform float uThick;\n"
    "varying vec2 fTexCoord;\n"
    "void main()\n"
    "{\n"
    "    float d = sqrt(dot(fTexCoord, fTexCoord));\n"
    "    float t = 1.0 - smoothstep(0.0, uThick, abs(1.0 - uThick - d));\n"
    "    gl_FragColor = vec4(uColor.rgb, t) * uColor.a;\n"
    "}\n";

static const char line_vs[] =
    "precision mediump float;\n"
    "attribute vec2 vPosition;\n"
    "uniform vec2 uAspectRatio;\n"
    "uniform vec2 uCenterPos;\n"
    "uniform vec2 uSize;\n"
    "uniform float uAngle;\n"
    "void main()\n"
    "{\n"
    "    vec2 pos = vPosition * uSize;\n"
    "    pos = mat2(\n"
    "      -cos(uAngle), sin(uAngle),\n"
    "      sin(uAngle), cos(uAngle)\n"
    "    ) * pos;\n"
    "    pos += uCenterPos;\n"
    "    pos /= uAspectRatio;\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "}\n";
static const char line_fs[] =
    "precision mediump float;\n"
    "uniform vec4 uColor;\n"
    "void main()\n"
    "{\n"
    "    gl_FragColor = uColor;\n"
    "}\n";

/* ------------------------------------------------------------------------- */
void gfx_gles2_debug_init(struct gfx_debug* debug)
{
    debug_circle_vec_init(&debug->circles);
    debug_rectangle_vec_init(&debug->rectangles);
    debug_line_vec_init(&debug->lines);
    strlist_init(&debug->strings);

    debug->circle_mat.program = INVALID_HANDLE;
    debug->circle_mat.uPosCameraSpace = INVALID_UNIFORM_LOCATION;
    debug->circle_mat.uAspectRatio = INVALID_UNIFORM_LOCATION;
    debug->circle_mat.uRadius = INVALID_UNIFORM_LOCATION;
    debug->circle_mat.uThick = INVALID_UNIFORM_LOCATION;
    debug->circle_mat.uColor = INVALID_UNIFORM_LOCATION;

    debug->line_mat.program = INVALID_HANDLE;
    debug->line_mat.uAspectRatio = INVALID_UNIFORM_LOCATION;
    debug->line_mat.uCenterPos = INVALID_UNIFORM_LOCATION;
    debug->line_mat.uSize = INVALID_UNIFORM_LOCATION;
    debug->line_mat.uAngle = INVALID_UNIFORM_LOCATION;
    debug->line_mat.uColor = INVALID_UNIFORM_LOCATION;
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_debug_deinit(struct gfx_debug* debug)
{
    if (debug->line_mat.program != INVALID_HANDLE)
    {
        gfx_untrack_shader(debug->line_mat.program);
        glDeleteProgram(debug->line_mat.program);
    }
    if (debug->circle_mat.program != INVALID_HANDLE)
    {
        gfx_untrack_shader(debug->circle_mat.program);
        glDeleteProgram(debug->circle_mat.program);
    }

    strlist_deinit(debug->strings);
    debug_line_vec_deinit(debug->lines);
    debug_rectangle_vec_deinit(debug->rectangles);
    debug_circle_vec_deinit(debug->circles);
}

/* ------------------------------------------------------------------------- */
int gfx_gles2_debug_load(struct gfx_debug* debug)
{
    static const char* attr_bindings[] = {"vPosition", NULL};
    debug->circle_mat.program =
        gfx_gles2_load_shader_str(circle_vs, circle_fs, attr_bindings);
    if (debug->circle_mat.program == INVALID_HANDLE)
        return -1;

    debug->line_mat.program =
        gfx_gles2_load_shader_str(line_vs, line_fs, attr_bindings);
    if (debug->line_mat.program == INVALID_HANDLE)
        return -1;

    debug->circle_mat.uAspectRatio = gfx_gles2_get_uniform_location_and_warn(
        debug->circle_mat.program, "uAspectRatio");
    debug->circle_mat.uPosCameraSpace = gfx_gles2_get_uniform_location_and_warn(
        debug->circle_mat.program, "uPosCameraSpace");
    debug->circle_mat.uRadius = gfx_gles2_get_uniform_location_and_warn(
        debug->circle_mat.program, "uRadius");
    debug->circle_mat.uThick = gfx_gles2_get_uniform_location_and_warn(
        debug->circle_mat.program, "uThick");
    debug->circle_mat.uColor = gfx_gles2_get_uniform_location_and_warn(
        debug->circle_mat.program, "uColor");

    debug->line_mat.uAspectRatio = gfx_gles2_get_uniform_location_and_warn(
        debug->line_mat.program, "uAspectRatio");
    debug->line_mat.uCenterPos = gfx_gles2_get_uniform_location_and_warn(
        debug->line_mat.program, "uCenterPos");
    debug->line_mat.uSize = gfx_gles2_get_uniform_location_and_warn(
        debug->line_mat.program, "uSize");
    debug->line_mat.uAngle = gfx_gles2_get_uniform_location_and_warn(
        debug->line_mat.program, "uAngle");
    debug->line_mat.uColor = gfx_gles2_get_uniform_location_and_warn(
        debug->line_mat.program, "uColor");

    return 0;
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_debug_unload(struct gfx_debug* debug)
{
    if (debug->circle_mat.program != INVALID_HANDLE)
    {
        gfx_untrack_shader(debug->circle_mat.program);
        glDeleteProgram(debug->circle_mat.program);
    }
    debug->circle_mat.program = INVALID_HANDLE;
}

/* ------------------------------------------------------------------------- */
static void draw_circle(
    const struct gfx_debug*    dbg,
    const struct debug_circle* circle,
    const struct camera*       camera,
    GLfloat                    pixel_size)
{
    GLfloat x, y;
    x = qw_to_float(qw_sub(circle->pos.x, camera->pos.x)) *
        qw_to_float(camera->scale);
    y = qw_to_float(qw_sub(circle->pos.y, camera->pos.y)) *
        qw_to_float(camera->scale);

    glUniform1f(dbg->circle_mat.uRadius, qw_to_float(circle->radius));
    glUniform1f(
        dbg->circle_mat.uThick,
        pixel_size / qw_to_float(circle->radius) / qw_to_float(camera->scale));
    glUniform3f(
        dbg->circle_mat.uPosCameraSpace, x, y, qw_to_float(camera->scale));
    glUniform4f(
        dbg->circle_mat.uColor,
        ((circle->argb >> 16) & 0xFF) / 255.0,
        ((circle->argb >> 8) & 0xFF) / 255.0,
        ((circle->argb >> 0) & 0xFF) / 255.0,
        (circle->argb >> 24) / 255.0);

    gfx_gles2_quad_mesh_draw();
}

/* ------------------------------------------------------------------------- */
static void draw_line(
    const struct gfx_debug* dbg,
    struct qwpos            start,
    struct qwpos            end,
    uint32_t                argb,
    const struct camera*    camera,
    GLfloat                 pixel_size)
{
    GLfloat x1, y1, x2, y2, dx, dy, d, a;

    x1 = qw_to_float(qw_sub(start.x, camera->pos.x)) *
         qw_to_float(camera->scale);
    y1 = qw_to_float(qw_sub(start.y, camera->pos.y)) *
         qw_to_float(camera->scale);
    x2 = qw_to_float(qw_sub(end.x, camera->pos.x)) * qw_to_float(camera->scale);
    y2 = qw_to_float(qw_sub(end.y, camera->pos.y)) * qw_to_float(camera->scale);

    dx = x2 - x1;
    dy = y2 - y1;
    d = sqrt(dx * dx + dy * dy) / 2; /* quad mesh is [-1, 1], div by 2 */
    a = atan2(dx, dy);

    glUniform2f(dbg->line_mat.uCenterPos, (x1 + x2) / 2, (y1 + y2) / 2);
    glUniform2f(dbg->line_mat.uSize, pixel_size, d);
    glUniform1f(dbg->line_mat.uAngle, a);
    glUniform4f(
        dbg->line_mat.uColor,
        (argb >> 16) / 255.0,
        ((argb >> 8) & 0xFF) / 255.0,
        ((argb >> 0) & 0xFF) / 255.0,
        (argb >> 24) / 255.0);

    gfx_gles2_quad_mesh_draw();
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_debug_draw(
    struct gfx*                 gfx,
    struct gfx_debug*           debug,
    const struct gfx_quad_mesh* mesh,
    const struct camera*        camera,
    const struct aspect_ratio*  ar)
{
    const struct debug_circle*    circle;
    const struct debug_line*      line;
    const struct debug_rectangle* rect;
    struct strview                str;
    GLfloat pixel_size_x, pixel_size_y, pixel_size, offset_y;
    int     i;

    pixel_size_x = 1.0 / gfx->width;
    pixel_size_y = 1.0 / gfx->height;
    pixel_size = pixel_size_x > pixel_size_y ? pixel_size_x : pixel_size_y;
    pixel_size *= 3; /* looks better */

    gfx_gles2_quad_mesh_prepare_draw(mesh);

    glUseProgram(debug->circle_mat.program);
    glUniform2f(debug->circle_mat.uAspectRatio, ar->scale_x, ar->scale_y);
    vec_for_each (debug->circles, circle)
        draw_circle(debug, circle, camera, pixel_size);

    glUseProgram(debug->line_mat.program);
    glUniform2f(debug->line_mat.uAspectRatio, ar->scale_x, ar->scale_y);
    vec_for_each (debug->lines, line)
    {
        draw_line(
            debug, line->start, line->end, line->argb, camera, pixel_size);
    }

    vec_for_each (debug->rectangles, rect)
    {
        struct qwpos p0 = rect->top_left;
        struct qwpos p1 = make_qwposqw(rect->bottom_right.x, rect->top_left.y);
        struct qwpos p2 = rect->bottom_right;
        struct qwpos p3 = make_qwposqw(rect->top_left.x, rect->bottom_right.y);
        draw_line(debug, p0, p1, rect->argb, camera, pixel_size);
        draw_line(debug, p1, p2, rect->argb, camera, pixel_size);
        draw_line(debug, p2, p3, rect->argb, camera, pixel_size);
        draw_line(debug, p3, p0, rect->argb, camera, pixel_size);
    }

    glUseProgram(0);
    gfx_gles2_quad_mesh_end_draw();

    offset_y = 0.95;
    gfx_gles2_text_prepare_draw(&gfx->font, ar);
    strlist_for_each (debug->strings, i, str)
    {
        offset_y -= 1.0 / 96;
        gfx_gles2_text_draw_screen(
            str,
            &gfx->font,
            make_fpos(-1.0, offset_y),
            1.0 / 96,
            0xFFFFFFFF,
            UI_ALIGN_LEFT);
    }
    gfx_gles2_text_end_draw();

    debug_circle_vec_clear(debug->circles);
    debug_rectangle_vec_clear(debug->rectangles);
    debug_line_vec_clear(debug->lines);
    strlist_clear(debug->strings);
}
