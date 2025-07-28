#include "./gfx.h"
#include "./shader.h"
#include "clither/game/camera.h"
#include "clither/game/resource_pack.h"
#include "clither/util/strlist.h"
#include "stb_image.h"

void gfx_gles2_sprite_mat_init(struct gfx_sprite_mat* mat)
{
    int i;

    mat->program = INVALID_HANDLE;
    mat->uAspectRatio = INVALID_UNIFORM_LOCATION;
    mat->uPosCameraSpace = INVALID_UNIFORM_LOCATION;
    mat->uDir = INVALID_UNIFORM_LOCATION;
    mat->uSize = INVALID_UNIFORM_LOCATION;
    mat->uAnim = INVALID_UNIFORM_LOCATION;

    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
        mat->sTex[i] = INVALID_UNIFORM_LOCATION;
}

void gfx_gles2_sprite_mat_deinit(struct gfx_sprite_mat* mat)
{
    if (mat->program != 0)
    {
        gfx_untrack_shader(mat->program);
        glDeleteProgram(mat->program);
    }
}

int gfx_gles2_sprite_mat_load(
    struct gfx_sprite_mat* mat, const struct resource_shader* shader)
{
    int i;

    CLITHER_DEBUG_ASSERT(mat->program == 0);
    mat->program =
        gfx_gles2_load_shader(shader->sprite, gfx_gles2_quad_attr_bindings);
    if (mat->program == 0)
        return -1;
    gfx_track_shader(mat->program);

    mat->uAspectRatio =
        gfx_gles2_get_uniform_location_and_warn(mat->program, "uAspectRatio");
    mat->uPosCameraSpace = gfx_gles2_get_uniform_location_and_warn(
        mat->program, "uPosCameraSpace");
    mat->uDir = gfx_gles2_get_uniform_location_and_warn(mat->program, "uDir");
    mat->uSize = gfx_gles2_get_uniform_location_and_warn(mat->program, "uSize");
    mat->uAnim = gfx_gles2_get_uniform_location_and_warn(mat->program, "uAnim");

    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
    {
        char uniform_name[16] = "sTexX";
        uniform_name[4] = '0' + i;
        mat->sTex[i] = glGetUniformLocation(mat->program, uniform_name);
    }

    return 0;
}

void gfx_gles2_sprite_mat_unload(struct gfx_sprite_mat* mat)
{
    if (mat->program != 0)
    {
        gfx_untrack_shader(mat->program);
        glDeleteProgram(mat->program);
        mat->program = 0;
    }
}

void gfx_gles2_sprite_tex_init(struct gfx_sprite_tex* tex)
{
    int i;
    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
        tex->tex[i] = INVALID_HANDLE;

    tex->scale = 1.0;
    tex->tile_x = 1;
    tex->tile_y = 1;
    tex->tile_count = 1;
    tex->fps = 0;
    tex->anim_frame = 0;
    tex->sim_time = 0;
}

void gfx_gles2_sprite_tex_deinit(struct gfx_sprite_tex* tex)
{
    (void)tex;
}

void gfx_gles2_sprite_tex_load(
    struct gfx_sprite_tex* tex, const struct resource_layer* res)
{
    int         i;
    int         img_width, img_height, img_channels;
    stbi_uc*    img_data;
    const char* tex_filename;

    strlist_for_each_cstr(res->textures, i, tex_filename)
    {
        log_dbg("Loading texture \"%s\"\n", tex_filename);
        img_data =
            stbi_load(tex_filename, &img_width, &img_height, &img_channels, 4);
        if (img_data == NULL)
        {
            log_warn("Failed to load image \"%s\"\n", tex_filename);
            continue;
        }

        glGenTextures(1, &tex->tex[i]);
        gfx_track_tex(tex->tex[i]);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex->tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
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
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(img_data);
    }

    tex->tile_x = res->tile_x;
    tex->tile_y = res->tile_y;
    tex->tile_count = res->num_frames;
    tex->scale = res->scale;
    tex->fps = res->fps;
    tex->sim_time = 0;
    tex->anim_frame = 0;
}

void gfx_gles2_sprite_tex_unload(struct gfx_sprite_tex* tex)
{
    int i;

    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
        if (tex->tex[i] != 0)
        {
            gfx_untrack_tex(tex->tex[i]);
            glDeleteTextures(1, &tex->tex[i]);
            tex->tex[i] = INVALID_HANDLE;
        }
}

void gfx_gles2_sprite_prepare_draw(
    const struct gfx_quad_mesh*  mesh,
    const struct gfx_sprite_mat* mat,
    const struct aspect_ratio*   ar)
{
    int i;

    gfx_gles2_quad_mesh_prepare_draw(mesh);

    glUseProgram(mat->program);
    glUniform2f(mat->uAspectRatio, ar->scale_x, ar->scale_y);

    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
        if (mat->sTex[i] != INVALID_UNIFORM_LOCATION)
            glUniform1i(mat->sTex[i], i);
}

int gfx_gles2_sprite_bind_textures(const struct gfx_sprite_tex* tex)
{
    int i;
    int bound = 0;
    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
        if (tex->tex[i] != INVALID_HANDLE)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, tex->tex[i]);
            bound = 1;
        }

    return bound;
}

void gfx_gles2_sprite_end_draw(void)
{
    glUseProgram(0);
    gfx_gles2_quad_mesh_end_draw();
}

int gfx_gles2_sprite_update_uniforms(
    const struct gfx_sprite_mat* mat,
    const struct gfx_sprite_tex* tex,
    struct qwpos                 pos,
    struct qwpos                 dir,
    qw                           scale,
    const struct camera*         camera)
{
    int tile_x, tile_y;
    struct
    {
        GLfloat x, y;
    } pos_cameraSpace;

    pos_cameraSpace.x = qw_to_float(pos.x) - qw_to_float(camera->pos.x);
    pos_cameraSpace.y = qw_to_float(pos.y) - qw_to_float(camera->pos.y);
    pos_cameraSpace.y *= qw_to_float(camera->scale);
    pos_cameraSpace.x *= qw_to_float(camera->scale);

    tile_x = tex->anim_frame % tex->tile_x;
    tile_y = (tex->anim_frame / tex->tile_x) % tex->tile_y;

    glUniform1f(mat->uSize, tex->scale * qw_to_float(scale));
    glUniform3f(
        mat->uPosCameraSpace,
        pos_cameraSpace.x,
        pos_cameraSpace.y,
        qw_to_float(camera->scale));
    glUniform2f(mat->uDir, qw_to_float(dir.x), qw_to_float(dir.y));
    glUniform4f(
        mat->uAnim,
        1.0 / tex->tile_x,
        1.0 / tex->tile_y,
        (GLfloat)tile_x / tex->tile_x,
        (GLfloat)tile_y / tex->tile_y);

    return 1;
}

void gfx_gles2_sprite_draw(void)
{
    gfx_gles2_quad_mesh_draw();
}

void gfx_gles2_sprite_step_anim(struct gfx_sprite_tex* tex, int sim_tick_rate)
{
    float anim_step = (float)sim_tick_rate / (float)tex->fps;
    tex->sim_time += 1.0;
    while (tex->sim_time >= anim_step)
    {
        tex->sim_time -= anim_step;
        tex->anim_frame++;
        if (tex->anim_frame >= tex->tile_count)
            tex->anim_frame = 0;
    }
}
