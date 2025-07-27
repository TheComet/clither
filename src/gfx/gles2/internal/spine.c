#include "./gfx.h"
#include "./shader.h"
#include "./spine.h"
#include "clither/game/bezier.h"
#include "clither/game/camera.h"
#include "clither/game/resource_pack.h"
#include "clither/util/strlist.h"
#include "stb_image.h"

#define QUADS 20

struct vertex
{
    GLfloat pos[2];
};

static struct vertex vertex(GLfloat x, GLfloat y)
{
    struct vertex v;
    v.pos[0] = x;
    v.pos[1] = y;
    return v;
}

static struct vertex spine_vertices[2 * QUADS + 2];
static const char*   attr_bindings[] = {"vPosition", NULL};

/* ------------------------------------------------------------------------- */
static void spine_vertices_init(void)
{
    int i;
    for (i = 0; i <= QUADS; ++i)
    {
        GLfloat x = -1.0f + (2.0f * i) / QUADS;
        spine_vertices[i * 2 + 0] = vertex(x, -1.0f);
        spine_vertices[i * 2 + 1] = vertex(x, 1.0f);
    }
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_spine_init(struct spine* spine)
{
    int i;
    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
    {
        spine->tex[i] = INVALID_HANDLE;
        spine->sTex[i] = INVALID_UNIFORM_LOCATION;
    }

    spine->program = INVALID_HANDLE;
    spine->uCoeff = INVALID_UNIFORM_LOCATION;
    spine->uWidth = INVALID_UNIFORM_LOCATION;
    spine->uAspectRatio = INVALID_UNIFORM_LOCATION;

    spine->width = 1.0;

    glGenBuffers(1, &spine->vbo);
    gfx_track_buf(spine->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, spine->vbo);
    spine_vertices_init();
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
int gfx_gles2_spine_load(
    struct spine*                 spine,
    const struct resource_spine*  res,
    const struct resource_shader* shader)
{
    int         i;
    int         img_width, img_height, img_channels;
    stbi_uc*    img_data;
    const char* tex_filename;

    spine->width = res->width;

    CLITHER_DEBUG_ASSERT(spine->program == 0);
    spine->program = gfx_gles2_load_shader(shader->spine, attr_bindings);
    if (spine->program == 0)
        return -1;
    gfx_track_shader(spine->program);
    spine->uCoeff =
        gfx_gles2_get_uniform_location_and_warn(spine->program, "uCoeff");
    spine->uWidth =
        gfx_gles2_get_uniform_location_and_warn(spine->program, "uWidth");
    spine->uAspectRatio =
        gfx_gles2_get_uniform_location_and_warn(spine->program, "uAspectRatio");
    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
    {
        char uniform_name[16] = "sTexX";
        uniform_name[4] = '0' + i;
        spine->sTex[i] = glGetUniformLocation(spine->program, uniform_name);
    }

    strlist_for_each (res->textures, i, tex_filename)
    {
        log_dbg("Loading texture \"%s\"\n", tex_filename);
        img_data =
            stbi_load(tex_filename, &img_width, &img_height, &img_channels, 4);
        if (img_data == NULL)
        {
            log_warn("Failed to load image \"%s\"\n", tex_filename);
            continue;
        }

        glGenTextures(1, &spine->tex[i]);
        gfx_track_tex(spine->tex[i]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, spine->tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
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
    const struct spine*        spine,
    const struct bezier_knot*  head,
    const struct bezier_knot*  tail,
    qw                         scale,
    const struct camera*       camera,
    const struct aspect_ratio* ar)
{
    struct
    {
        GLfloat x, y;
    } p0, p1, p2, p3, A[4];

    p0.x = qw_to_float(head->pos.x) - qw_to_float(camera->pos.x);
    p0.y = qw_to_float(head->pos.y) - qw_to_float(camera->pos.y);
    p3.x = qw_to_float(tail->pos.x) - qw_to_float(camera->pos.x);
    p3.y = qw_to_float(tail->pos.y) - qw_to_float(camera->pos.y);

    p1.x = p0.x + cos(qa_to_float(head->angle)) * head->len_backwards / 255.0;
    p1.y = p0.y + sin(qa_to_float(head->angle)) * head->len_backwards / 255.0;
    p2.x = p3.x - cos(qa_to_float(tail->angle)) * tail->len_forwards / 255.0;
    p2.y = p3.y - sin(qa_to_float(tail->angle)) * tail->len_forwards / 255.0;

    p0.x *= qw_to_float(camera->scale) * qw_to_float(scale) / ar->scale_x;
    p0.y *= qw_to_float(camera->scale) * qw_to_float(scale) / ar->scale_y;
    p1.x *= qw_to_float(camera->scale) * qw_to_float(scale) / ar->scale_x;
    p1.y *= qw_to_float(camera->scale) * qw_to_float(scale) / ar->scale_y;
    p2.x *= qw_to_float(camera->scale) * qw_to_float(scale) / ar->scale_x;
    p2.y *= qw_to_float(camera->scale) * qw_to_float(scale) / ar->scale_y;
    p3.x *= qw_to_float(camera->scale) * qw_to_float(scale) / ar->scale_x;
    p3.y *= qw_to_float(camera->scale) * qw_to_float(scale) / ar->scale_y;

    A[0].x = p0.x;
    A[1].x = 3 * p1.x - 3 * p0.x;
    A[2].x = 3 * p2.x - 6 * p1.x + 3 * p0.x;
    A[3].x = p3.x - 3 * p2.x + 3 * p1.x - p0.x;

    A[0].y = p0.y;
    A[1].y = 3 * p1.y - 3 * p0.y;
    A[2].y = 3 * p2.y - 6 * p1.y + 3 * p0.y;
    A[3].y = p3.y - 3 * p2.y + 3 * p1.y - p0.y;

    glUniform1f(spine->uWidth, qw_to_float(scale) * spine->width);
    glUniform2fv(spine->uCoeff, 4, (GLfloat*)A);
    glUniform2f(spine->uAspectRatio, ar->scale_x, ar->scale_y);

    glBindBuffer(GL_ARRAY_BUFFER, spine->vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(struct vertex),
        (void*)offsetof(struct vertex, pos));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 6 * QUADS);

    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_spine_end_draw(void)
{
    glUseProgram(0);
}
