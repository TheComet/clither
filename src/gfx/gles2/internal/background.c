#include "./gfx.h"
#include "./shader.h"
#include "clither/game/camera.h"
#include "clither/game/resource_pack.h"
#include "clither/game/resource_sprite_vec.h"
#include "clither/game/world.h"
#include "clither/util/strlist.h"
#include "stb_image.h"

int gfx_gles2_background_init(
    struct background* bg,
    int                fbwidth,
    int                fbheight,
    int                shadow_map_size_factor)
{
    memset(bg, 0, sizeof *bg);

    /* Set up shadow framebuffer */
    glGenTextures(1, &bg->texShadow);
    gfx_track_tex(bg->texShadow);
    glBindTexture(GL_TEXTURE_2D, bg->texShadow);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB,
        fbwidth / shadow_map_size_factor,
        fbheight / shadow_map_size_factor,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &bg->fbo);
    gfx_track_fbo(bg->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, bg->fbo);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bg->texShadow, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        log_err("Incomplete framebuffer!\n");
        goto incomplete_shadow_framebuffer;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /* Prepare background textures */
    glGenTextures(1, &bg->texCol);
    gfx_track_tex(bg->texCol);
    glBindTexture(GL_TEXTURE_2D, bg->texCol);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &bg->texNor);
    gfx_track_tex(bg->texNor);
    glBindTexture(GL_TEXTURE_2D, bg->texNor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Set default values for shader uniforms */
    bg->program = 0;
    bg->uAspectRatio = (GLuint)-1;
    bg->uCamera = (GLuint)-1;
    bg->uShadowInvRes = (GLuint)-1;
    bg->uWorldBorder = (GLuint)-1;
    bg->sShadow = (GLuint)-1;
    bg->sCol = (GLuint)-1;
    bg->sNM = (GLuint)-1;

    return 0;

incomplete_shadow_framebuffer:
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &bg->fbo);
    glDeleteTextures(1, &bg->texShadow);

    return -1;
}

void gfx_gles2_background_deinit(struct background* bg)
{
    gfx_untrack_tex(bg->texNor);
    glDeleteTextures(1, &bg->texNor);

    gfx_untrack_tex(bg->texCol);
    glDeleteTextures(1, &bg->texCol);

    gfx_untrack_fbo(bg->fbo);
    glDeleteFramebuffers(1, &bg->fbo);

    gfx_untrack_tex(bg->texShadow);
    glDeleteTextures(1, &bg->texShadow);

    if (bg->program != 0)
    {
        gfx_untrack_shader(bg->program);
        glDeleteProgram(bg->program);
    }
}

void gfx_gles2_background_resize(
    struct background* bg,
    int                fbwidth,
    int                fbheight,
    int                shadow_map_size_factor)
{
    glBindTexture(GL_TEXTURE_2D, bg->texShadow);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB,
        fbwidth / shadow_map_size_factor,
        fbheight / shadow_map_size_factor,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, bg->fbo);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bg->texShadow, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

int gfx_gles2_background_load(
    struct background* bg, const struct resource_pack* pack)
{
    int             img_width, img_height, img_channels;
    stbi_uc*        img_data;
    struct strlist* textures;

    /* For now we only support a single background layer */
    if (vec_count(pack->sprites.background) == 0)
    {
        log_warn("No background texture defined\n");
        return -1;
    }

    /* Load shaders */
    CLITHER_DEBUG_ASSERT(bg->program == 0);
    bg->program = gfx_gles2_load_shader(
        pack->shaders.glsl.background, gfx_gles2_quad_attr_bindings);
    if (bg->program == 0)
        return -1;
    gfx_track_shader(bg->program);

    bg->uAspectRatio =
        gfx_gles2_get_uniform_location_and_warn(bg->program, "uAspectRatio");
    bg->uCamera =
        gfx_gles2_get_uniform_location_and_warn(bg->program, "uCamera");
    bg->uShadowInvRes =
        gfx_gles2_get_uniform_location_and_warn(bg->program, "uShadowInvRes");
    bg->uWorldBorder =
        gfx_gles2_get_uniform_location_and_warn(bg->program, "uWorldBorder");
    bg->sShadow =
        gfx_gles2_get_uniform_location_and_warn(bg->program, "sShadow");
    bg->sCol = gfx_gles2_get_uniform_location_and_warn(bg->program, "sCol");
    bg->sNM = gfx_gles2_get_uniform_location_and_warn(bg->program, "sNM");

    textures = (*vec_first(pack->sprites.background))->textures;
    if (strlist_count(textures) > 0)
    {
        img_data = stbi_load(
            strlist_cstr(textures, 0),
            &img_width,
            &img_height,
            &img_channels,
            3);
        if (img_data)
        {
            glBindTexture(GL_TEXTURE_2D, bg->texCol);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGB,
                img_width,
                img_height,
                0,
                GL_RGB,
                GL_UNSIGNED_BYTE,
                img_data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(img_data);
        }
        else
            log_warn(
                "Failed to load image \"%s\"\n", strlist_cstr(textures, 0));
    }

    if (strlist_count(textures) > 1)
    {
        img_data = stbi_load(
            strlist_cstr(textures, 1),
            &img_width,
            &img_height,
            &img_channels,
            3);
        if (img_data)
        {
            glBindTexture(GL_TEXTURE_2D, bg->texNor);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGB,
                img_width,
                img_height,
                0,
                GL_RGB,
                GL_UNSIGNED_BYTE,
                img_data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(img_data);
        }
        else
            log_warn(
                "Failed to load image \"%s\"\n", strlist_cstr(textures, 1));
    }

    return 0;
}

void gfx_gles2_background_unload(struct background* bg)
{
    if (bg->program != 0)
    {
        gfx_untrack_shader(bg->program);
        glDeleteProgram(bg->program);
        bg->program = 0;
    }

    glBindTexture(GL_TEXTURE_2D, bg->texCol);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, bg->texNor);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void gfx_gles2_background_draw(
    const struct world*        world,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar,
    int                        shadow_map_size_factor)
{
    gfx_gles2_quad_mesh_prepare_draw(&gfx->quad_mesh);
    glUseProgram(gfx->background.program);
    glBindTexture(GL_TEXTURE_2D, gfx->background.texShadow);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gfx->background.texCol);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gfx->background.texNor);

    glUniform4f(
        gfx->background.uAspectRatio,
        ar->scale_x,
        ar->scale_y,
        ar->pad_x,
        ar->pad_y);
    glUniform3f(
        gfx->background.uCamera,
        qw_to_float(camera->pos.x),
        qw_to_float(camera->pos.y),
        qw_to_float(camera->scale));
    glUniform2f(
        gfx->background.uShadowInvRes,
        (GLfloat)shadow_map_size_factor / gfx->width,
        (GLfloat)shadow_map_size_factor / gfx->height);
    glUniform3f(
        gfx->background.uWorldBorder,
        qw_to_float(world->inner_radius),
        qw_to_float(world->ring_start),
        qw_to_float(world->ring_end));
    glUniform1i(gfx->background.sCol, 1);
    glUniform1i(gfx->background.sNM, 2);

    gfx_gles2_quad_mesh_draw();

    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    gfx_gles2_quad_mesh_end_draw();
}
