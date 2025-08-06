#include "./gfx.h"
#include "./shader.h"
#include "./spine.h"
#include "clither/game/bezier.h"
#include "clither/game/bezier_segment_rb.h"
#include "clither/game/camera.h"
#include "clither/game/resource_pack.h"
#include "clither/util/strlist.h"
#include "stb_image.h"

#define QUADS 8

struct vertex
{
    GLfloat pos[2];
};

static struct vertex vertex(GLfloat x, GLfloat y)
{
    struct vertex vert;
    vert.pos[0] = x;
    vert.pos[1] = y;
    return vert;
}

static const char* attr_bindings[] = {"vPosition", NULL};

/* ------------------------------------------------------------------------- */
void gfx_gles2_spine_init(struct spine* spine)
{
    int           i;
    struct vertex spine_vertices[2 * QUADS + 2];

    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
    {
        spine->tex[i] = INVALID_HANDLE;
        spine->sTex[i] = INVALID_UNIFORM_LOCATION;
    }

    spine->program = INVALID_HANDLE;
    spine->uCoeff = INVALID_UNIFORM_LOCATION;
    spine->uBezierSize = INVALID_UNIFORM_LOCATION;
    spine->uAspectRatio = INVALID_UNIFORM_LOCATION;
    spine->uHeadPosition = INVALID_UNIFORM_LOCATION;
    spine->uScrollScaleOffset = INVALID_UNIFORM_LOCATION;
    spine->uCutoff = INVALID_UNIFORM_LOCATION;

    spine->spine_width = 1.0;
    spine->tex_aspect_ratio = 0;

    for (i = 0; i <= QUADS; ++i)
    {
        GLfloat x = (GLfloat)i / QUADS;
        spine_vertices[i * 2 + 0] = vertex(x, -1);
        spine_vertices[i * 2 + 1] = vertex(x, 1);
    }
    glGenBuffers(1, &spine->vbo);
    gfx_track_buf(spine->vbo, "spine->vbo");
    glBindBuffer(GL_ARRAY_BUFFER, spine->vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(spine_vertices),
        spine_vertices,
        GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_spine_deinit(struct spine* spine)
{
    gfx_untrack_buf(spine->vbo);
    glDeleteBuffers(1, &spine->vbo);

    if (spine->program != 0)
    {
        gfx_untrack_shader(spine->program);
        glDeleteProgram(spine->program);
    }
}

/* ------------------------------------------------------------------------- */
static GLfloat bezier_length_f(const struct bezier_segment* segment)
{
    int     v;
    GLfloat last_x = 0.0, last_y = 0.0;
    GLfloat length = 0.0;
    for (v = 1; v <= QUADS; ++v)
    {
        qw           t = make_qw2(v, QUADS);
        struct qwpos pos = bezier_xy(segment, t);
        GLfloat      pos_x = qw_to_float(pos.x);
        GLfloat      pos_y = qw_to_float(pos.y);
        GLfloat      dx = pos_x - last_x;
        GLfloat      dy = pos_y - last_y;
        length += sqrtf(dx * dx + dy * dy);
        last_x = pos_x;
        last_y = pos_y;
    }

    return length;
}

/* ------------------------------------------------------------------------- */
int gfx_gles2_spine_load(
    struct spine*                 spine,
    const struct resource_spine*  res,
    const struct resource_shader* shader)
{
    int         i;
    int         img_width, img_height, img_channels;
    stbi_uc*    img_data;
    const char* tex_filename;

    spine->spine_width = res->width;

    CLITHER_DEBUG_ASSERT(spine->program == 0);
    spine->program = gfx_gles2_load_shader(shader->spine, attr_bindings);
    if (spine->program == 0)
        return -1;

    spine->uCoeff =
        gfx_gles2_get_uniform_location_and_warn(spine->program, "uCoeff");
    spine->uBezierSize =
        gfx_gles2_get_uniform_location_and_warn(spine->program, "uBezierSize");
    spine->uAspectRatio =
        gfx_gles2_get_uniform_location_and_warn(spine->program, "uAspectRatio");
    spine->uHeadPosition = gfx_gles2_get_uniform_location_and_warn(
        spine->program, "uHeadPosition");
    spine->uScrollScaleOffset = gfx_gles2_get_uniform_location_and_warn(
        spine->program, "uScrollScaleOffset");
    spine->uCutoff =
        gfx_gles2_get_uniform_location_and_warn(spine->program, "uCutoff");

    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
    {
        char uniform_name[16] = "sTexX";
        uniform_name[4] = '0' + i;
        spine->sTex[i] = glGetUniformLocation(spine->program, uniform_name);
    }

    strlist_for_each_cstr (res->textures, i, tex_filename)
    {
        log_dbg("Loading texture \"%s\"\n", tex_filename);
        img_data =
            stbi_load(tex_filename, &img_width, &img_height, &img_channels, 4);
        if (img_data == NULL)
        {
            log_warn("Failed to load image \"%s\"\n", tex_filename);
            continue;
        }

        spine->tex_aspect_ratio = (GLfloat)img_height / (GLfloat)img_width;

        glGenTextures(1, &spine->tex[i]);
        gfx_track_tex(spine->tex[i], "spine->tex[i]");
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, spine->tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            img_width,
            img_height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            img_data);

        stbi_image_free(img_data);
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_spine_unload(struct spine* spine)
{
    int i;
    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
        if (spine->tex[i] != INVALID_HANDLE)
        {
            gfx_untrack_tex(spine->tex[i]);
            glDeleteTextures(1, &spine->tex[i]);
            spine->tex[i] = INVALID_HANDLE;
        }

    if (spine->program != 0)
    {
        gfx_untrack_shader(spine->program);
        glDeleteProgram(spine->program);
        spine->program = 0;
    }
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_spine_prepare_draw(const struct spine* spine)
{
    int i;

    glUseProgram(spine->program);

    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
        if (spine->sTex[i] != INVALID_UNIFORM_LOCATION)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, spine->tex[i]);
            glUniform1i(spine->sTex[i], i);
        }
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_spine_draw(
    const struct spine*             spine,
    const struct bezier_segment_rb* segments,
    qw                              snake_scale,
    qw                              snake_length,
    const struct camera*            camera,
    const struct aspect_ratio*      ar)
{
    int                          i, c;
    const struct bezier_segment* segment;
    GLfloat                      coeff[6];
    GLfloat                      scroll_offset;
    GLfloat                      total_length;
    GLfloat width = spine->spine_width * qw_to_float(snake_scale);
    GLfloat snake_length_f = qw_to_float(snake_length);

    glBindBuffer(GL_ARRAY_BUFFER, spine->vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(struct vertex),
        (void*)offsetof(struct vertex, pos));

    glUniform3f(
        spine->uAspectRatio,
        ar->scale_x,
        ar->scale_y,
        qw_to_float(camera->scale));

    scroll_offset = 0.0;
    total_length = 0.0;
    rb_for_each_r (segments, i, segment)
    {
        GLfloat length = bezier_length_f(segment);
        GLfloat bezier_aspect = length / width;

        GLfloat head_x =
            qw_to_float(segment->p[0].x) - qw_to_float(camera->pos.x);
        GLfloat head_y =
            qw_to_float(segment->p[0].y) - qw_to_float(camera->pos.y);
        for (c = 0; c != 3; c++)
        {
            coeff[c * 2 + 0] = q16_16_to_float(segment->coeff_x[c]);
            coeff[c * 2 + 1] = q16_16_to_float(segment->coeff_y[c]);
        }

        glUniform2fv(spine->uCoeff, 3, coeff);
        glUniform2f(spine->uBezierSize, width, length);
        glUniform2f(spine->uHeadPosition, head_x, head_y);
        glUniform2f(
            spine->uScrollScaleOffset,
            bezier_aspect * spine->tex_aspect_ratio,
            scroll_offset);
        glUniform1f(
            spine->uCutoff, snake_length_f / width * spine->tex_aspect_ratio);

        scroll_offset += bezier_aspect * spine->tex_aspect_ratio;
        total_length += length;

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 2 * QUADS + 2);

        if (total_length > snake_length_f)
            break;
    }

    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_spine_end_draw(void)
{
    glUseProgram(0);
}
