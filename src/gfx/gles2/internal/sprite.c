#include "./gfx.h"
#include "./shader.h"
#include "clither/game/camera.h"
#include "clither/game/resource_pack.h"
#include "clither/util/strlist.h"
#include "stb_image.h"

void gfx_gles2_sprite_mat_init(struct sprite_mat* mat)
{
    mat->program = 0;
    mat->uAspectRatio = (GLuint)-1;
    mat->uPosCameraSpace = (GLuint)-1;
    mat->uDir = (GLuint)-1;
    mat->uSize = (GLuint)-1;
    mat->uAnim = (GLuint)-1;
    mat->sCol = (GLuint)-1;
    mat->sNM = (GLuint)-1;
}

void gfx_gles2_sprite_mat_deinit(struct sprite_mat* mat)
{
    if (mat->program != 0)
        glDeleteProgram(mat->program);
}

int gfx_gles2_sprite_mat_load(
    struct sprite_mat* mat, const struct resource_pack* pack)
{
    CLITHER_DEBUG_ASSERT(mat->program == 0);
    mat->program = gfx_gles2_load_shader(
        pack->shaders.glsl.sprite, gfx_gles2_quad_attr_bindings);
    if (mat->program == 0)
        return -1;

    mat->uAspectRatio =
        gfx_gles2_get_uniform_location_and_warn(mat->program, "uAspectRatio");
    mat->uPosCameraSpace = gfx_gles2_get_uniform_location_and_warn(
        mat->program, "uPosCameraSpace");
    mat->uDir = gfx_gles2_get_uniform_location_and_warn(mat->program, "uDir");
    mat->uSize = gfx_gles2_get_uniform_location_and_warn(mat->program, "uSize");
    mat->uAnim = gfx_gles2_get_uniform_location_and_warn(mat->program, "uAnim");
    mat->sCol = gfx_gles2_get_uniform_location_and_warn(mat->program, "sCol");
    mat->sNM = gfx_gles2_get_uniform_location_and_warn(mat->program, "sNM");

    return 0;
}

void gfx_gles2_sprite_mat_unload(struct sprite_mat* mat)
{
    if (mat->program != 0)
        glDeleteProgram(mat->program);
    mat->program = 0;
}

void gfx_gles2_sprite_tex_init(struct sprite_tex* tex)
{
    glGenTextures(1, &tex->texDiffuse);
    glBindTexture(GL_TEXTURE_2D, tex->texDiffuse);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &tex->texNM);
    glBindTexture(GL_TEXTURE_2D, tex->texNM);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void gfx_gles2_sprite_tex_deinit(struct sprite_tex* tex)
{
    glDeleteTextures(1, &tex->texNM);
    glDeleteTextures(1, &tex->texDiffuse);
}

void gfx_gles2_sprite_tex_load(
    struct sprite_tex* tex, const struct resource_sprite* res)
{
    int      img_width, img_height, img_channels;
    stbi_uc* img_data;

    if (res == NULL)
    {
        log_warn("Sprite texture resource is NULL\n");
        return;
    }

    if (strlist_count(res->textures) > 0)
    {
        img_data = stbi_load(
            strlist_cstr(res->textures, 0),
            &img_width,
            &img_height,
            &img_channels,
            4);
        if (img_data != NULL)
        {
            glBindTexture(GL_TEXTURE_2D, tex->texDiffuse);
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
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(img_data);
        }
        else
            log_warn(
                "Failed to load image \"%s\"\n",
                strlist_cstr(res->textures, 0));
    }

    if (strlist_count(res->textures) > 1)
    {
        img_data = stbi_load(
            strlist_cstr(res->textures, 1),
            &img_width,
            &img_height,
            &img_channels,
            4);
        if (img_data != NULL)
        {
            glBindTexture(GL_TEXTURE_2D, tex->texNM);
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
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        else
            log_warn(
                "Failed to load image \"%s\"\n",
                strlist_cstr(res->textures, 1));
    }

    tex->tile_x = res->tile_x;
    tex->tile_y = res->tile_y;
    tex->tile_count = res->num_frames;
    tex->scale = res->scale;
    tex->fps = res->fps;
    tex->sim_time = 0;
    tex->anim_frame = 0;
}

void gfx_gles2_sprite_tex_unload(struct sprite_tex* tex)
{
    glBindTexture(GL_TEXTURE_2D, tex->texNM);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, tex->texDiffuse);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void gfx_gles2_sprite_prepare_draw(
    const struct quad_mesh*    mesh,
    const struct sprite_mat*   mat,
    const struct aspect_ratio* ar)
{
    gfx_gles2_quad_mesh_prepare_draw(mesh);
    glUseProgram(mat->program);
    glUniform2f(mat->uAspectRatio, ar->scale_x, ar->scale_y);
    glUniform1i(mat->sCol, 0);
    glUniform1i(mat->sNM, 1);
}

void gfx_gles2_sprite_bind_textures(const struct sprite_tex* tex)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex->texDiffuse);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex->texNM);
}

void gfx_gles2_sprite_end_draw(void)
{
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    gfx_gles2_quad_mesh_end_draw();
}

void gfx_gles2_sprite_update_uniforms(
    const struct sprite_mat* mat,
    const struct sprite_tex* tex,
    struct qwpos             pos,
    struct qwpos             dir,
    qw                       scale,
    const struct camera*     camera)
{
    int          tile_x, tile_y;
    struct qwpos pos_cameraSpace;

    pos_cameraSpace.x = qw_mul(qw_sub(pos.x, camera->pos.x), camera->scale);
    pos_cameraSpace.y = qw_mul(qw_sub(pos.y, camera->pos.y), camera->scale);

    tile_x = tex->anim_frame % tex->tile_x;
    tile_y = (tex->anim_frame / tex->tile_x) % tex->tile_y;

    glUniform1f(mat->uSize, tex->scale * qw_to_float(scale));
    glUniform3f(
        mat->uPosCameraSpace,
        qw_to_float(pos_cameraSpace.x),
        qw_to_float(pos_cameraSpace.y),
        qw_to_float(camera->scale));
    glUniform2f(mat->uDir, qw_to_float(dir.x), qw_to_float(dir.y));
    glUniform4f(
        mat->uAnim,
        1.0 / tex->tile_x,
        1.0 / tex->tile_y,
        (GLfloat)tile_x / tex->tile_x,
        (GLfloat)tile_y / tex->tile_y);
}

void gfx_gles2_sprite_draw(void)
{
    gfx_gles2_quad_mesh_draw();
}

void gfx_gles2_step_sprite_anim(struct sprite_tex* tex, int sim_tick_rate)
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
