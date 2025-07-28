#include "./gfx.h"
#include "./shader.h"
#include "clither/game/camera.h"
#include "clither/game/resource_pack.h"

void gfx_gles2_sprite_shadow_init(struct gfx_sprite_shadow_mat* ss)
{
    int i;

    ss->program = 0;
    ss->uAspectRatio = (GLuint)-1;
    ss->uPosCameraSpace = (GLuint)-1;
    ss->uDir = (GLuint)-1;
    ss->uSize = (GLuint)-1;
    ss->uAnim = (GLuint)-1;

    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
        ss->sTex[i] = INVALID_UNIFORM_LOCATION;
}

void gfx_gles2_sprite_shadow_deinit(struct gfx_sprite_shadow_mat* ss)
{
    if (ss->program != 0)
        glDeleteProgram(ss->program);
}

int gfx_gles2_sprite_shadow_load(
    struct gfx_sprite_shadow_mat* ss, const struct resource_shader* res)
{
    int i;

    CLITHER_DEBUG_ASSERT(ss->program == 0);
    ss->program =
        gfx_gles2_load_shader(res->shadow, gfx_gles2_quad_attr_bindings);
    if (ss->program == 0)
        return -1;
    gfx_track_shader(ss->program);

    ss->uAspectRatio =
        gfx_gles2_get_uniform_location_and_warn(ss->program, "uAspectRatio");
    ss->uPosCameraSpace =
        gfx_gles2_get_uniform_location_and_warn(ss->program, "uPosCameraSpace");
    ss->uDir = gfx_gles2_get_uniform_location_and_warn(ss->program, "uDir");
    ss->uSize = gfx_gles2_get_uniform_location_and_warn(ss->program, "uSize");
    ss->uAnim = gfx_gles2_get_uniform_location_and_warn(ss->program, "uAnim");

    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
    {
        char uniform_name[16] = "sTexX";
        uniform_name[4] = '0' + i;
        ss->sTex[i] = glGetUniformLocation(ss->program, uniform_name);
    }

    return 0;
}

void gfx_gles2_sprite_shadow_unload(struct gfx_sprite_shadow_mat* ss)
{
    if (ss->program != 0)
    {
        gfx_untrack_shader(ss->program);
        glDeleteProgram(ss->program);
        ss->program = 0;
    }
}

void gfx_gles2_sprite_shadow_prepare_draw(
    const struct gfx_background*        bg,
    const struct gfx_quad_mesh*         mesh,
    const struct gfx_sprite_shadow_mat* mat,
    const struct aspect_ratio*      ar,
    GLint                           gfx_width,
    GLint                           gfx_height,
    int                             shadow_map_size_factor)
{
    int i;

    gfx_gles2_quad_mesh_prepare_draw(mesh);

    glUseProgram(mat->program);
    glUniform2f(mat->uAspectRatio, ar->scale_x, ar->scale_y);

    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
        if (mat->sTex[i] != INVALID_UNIFORM_LOCATION)
            glUniform1i(mat->sTex[i], i);

    glBindFramebuffer(GL_FRAMEBUFFER, bg->fbo);
    glViewport(
        0,
        0,
        gfx_width / shadow_map_size_factor,
        gfx_height / shadow_map_size_factor);
}

void gfx_gles2_sprite_shadow_end_draw(GLint gfx_width, GLint gfx_height)
{
    glViewport(0, 0, gfx_width, gfx_height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);

    gfx_gles2_quad_mesh_end_draw();
}

void gfx_gles2_sprite_shadow_bind_textures(const struct gfx_sprite_tex* tex)
{
    int i;
    for (i = 0; i < MAX_TEXTURE_SAMPLERS; ++i)
        if (tex->tex[i] != INVALID_HANDLE)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, tex->tex[i]);
        }
}

void gfx_gles2_sprite_shadow_update_uniforms(
    const struct gfx_sprite_shadow_mat* mat,
    const struct gfx_sprite_tex*        tex,
    struct qwpos                    pos,
    struct qwpos                    dir,
    qw                              scale,
    const struct camera*            camera)
{
    int          tile_x, tile_y;
    struct qwpos pos_cameraSpace;

    pos_cameraSpace.x = qw_mul(qw_sub(pos.x, camera->pos.x), camera->scale);
    pos_cameraSpace.y = qw_mul(qw_sub(pos.y, camera->pos.y), camera->scale);

    /* Drop shadow */
    pos_cameraSpace.x =
        qw_sub(pos_cameraSpace.x, qw_mul(make_qw2(1, 128), camera->scale));
    pos_cameraSpace.y =
        qw_sub(pos_cameraSpace.y, qw_mul(make_qw2(1, 64), camera->scale));

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

void gfx_gles2_sprite_shadow_draw(void)
{
    gfx_gles2_quad_mesh_draw();
}
