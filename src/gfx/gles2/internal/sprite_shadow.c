#include "./gfx.h"
#include "./shader.h"
#include "clither/game/camera.h"
#include "clither/game/resource_pack.h"

void gfx_gles2_sprite_shadow_init(struct sprite_shadow_mat* ss)
{
    ss->program = 0;
    ss->uAspectRatio = (GLuint)-1;
    ss->uPosCameraSpace = (GLuint)-1;
    ss->uDir = (GLuint)-1;
    ss->uSize = (GLuint)-1;
    ss->uAnim = (GLuint)-1;
    ss->sNM = (GLuint)-1;
}

void gfx_gles2_sprite_shadow_deinit(struct sprite_shadow_mat* ss)
{
    if (ss->program != 0)
        glDeleteProgram(ss->program);
}

int gfx_gles2_sprite_shadow_load(
    struct sprite_shadow_mat* ss, const struct resource_pack* pack)
{
    CLITHER_DEBUG_ASSERT(ss->program == 0);
    ss->program = gfx_gles2_load_shader(
        pack->shaders.glsl.shadow, gfx_gles2_quad_attr_bindings);
    if (ss->program == 0)
        return -1;

    ss->uAspectRatio =
        gfx_gles2_get_uniform_location_and_warn(ss->program, "uAspectRatio");
    ss->uPosCameraSpace =
        gfx_gles2_get_uniform_location_and_warn(ss->program, "uPosCameraSpace");
    ss->uDir = gfx_gles2_get_uniform_location_and_warn(ss->program, "uDir");
    ss->uSize = gfx_gles2_get_uniform_location_and_warn(ss->program, "uSize");
    ss->uAnim = gfx_gles2_get_uniform_location_and_warn(ss->program, "uAnim");
    ss->sNM = gfx_gles2_get_uniform_location_and_warn(ss->program, "sNM");

    return 0;
}

void gfx_gles2_sprite_shadow_unload(struct sprite_shadow_mat* ss)
{
    if (ss->program != 0)
        glDeleteProgram(ss->program);
    ss->program = 0;
}

void gfx_gles2_sprite_shadow_prepare_draw(
    const struct background*        bg,
    const struct quad_mesh*         mesh,
    const struct sprite_shadow_mat* mat,
    const struct aspect_ratio*      ar,
    GLint                           gfx_width,
    GLint                           gfx_height,
    int                             shadow_map_size_factor)
{
    const GLint nmUnits[4] = {0, 1, 2, 3};

    gfx_gles2_quad_mesh_prepare_draw(mesh);

    glUseProgram(mat->program);
    glUniform2f(mat->uAspectRatio, ar->scale_x, ar->scale_y);
    glUniform1iv(mat->sNM, 4, nmUnits);

    glBindFramebuffer(GL_FRAMEBUFFER, bg->fbo);
    glViewport(
        0,
        0,
        gfx_width / shadow_map_size_factor,
        gfx_height / shadow_map_size_factor);
}

void gfx_gles2_sprite_shadow_end_draw(GLint gfx_width, GLint gfx_height)
{
    glBindTexture(GL_TEXTURE_2D, 0);
    glViewport(0, 0, gfx_width, gfx_height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    gfx_gles2_quad_mesh_end_draw();
}

void gfx_gles2_sprite_shadow_bind_textures(const struct sprite_tex* tex)
{
    glBindTexture(GL_TEXTURE_2D, tex->texNM);
}

void gfx_gles2_sprite_shadow_update_uniforms(
    const struct sprite_shadow_mat* mat,
    const struct sprite_tex*        tex,
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
