#include "./gfx.h"
#include "./gfx_constants.h"
#include "./shader.h"
#include "clither/game/camera.h"
#include "clither/game/resource_pack.h"
#include "clither/game/world.h"
#include "clither/util/strlist.h"
#include "stb_image.h"

int gfx_gles2_background_init(
    struct gfx_background* bg,
    int                fbwidth,
    int                fbheight,
    int                shadow_map_size_factor)
{
    int i;

    memset(bg, 0, sizeof *bg);

    /* Set up shadow framebuffer */
    glGenTextures(1, &bg->texShadow);
    gfx_track_tex(bg->texShadow);
    glActiveTexture(GL_TEXTURE0);
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

    /* Set default values for shader uniforms */
    bg->program = INVALID_HANDLE;
    bg->uAspectRatio = INVALID_UNIFORM_LOCATION;
    bg->uCamera = INVALID_UNIFORM_LOCATION;
    bg->uShadowInvRes = INVALID_UNIFORM_LOCATION;
    bg->uWorldBorder = INVALID_UNIFORM_LOCATION;
    bg->sShadow = INVALID_UNIFORM_LOCATION;

    for (i = 0; i != MAX_TEXTURE_SAMPLERS; ++i)
    {
        bg->tex[i] = INVALID_HANDLE;
        bg->sTex[i] = INVALID_UNIFORM_LOCATION;
    }

    return 0;

incomplete_shadow_framebuffer:
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &bg->fbo);
    glDeleteTextures(1, &bg->texShadow);

    return -1;
}

void gfx_gles2_background_deinit(struct gfx_background* bg)
{
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
    struct gfx_background* bg,
    int                fbwidth,
    int                fbheight,
    int                shadow_map_size_factor)
{
    glActiveTexture(GL_TEXTURE0);
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

    glBindFramebuffer(GL_FRAMEBUFFER, bg->fbo);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bg->texShadow, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

int gfx_gles2_background_load(
    struct gfx_background*                bg,
    const struct resource_background* res,
    const struct resource_shader*     shader)
{
    int         i;
    int         img_width, img_height, img_channels;
    const char* tex_filename;

    CLITHER_DEBUG_ASSERT(bg->program == 0);
    bg->program =
        gfx_gles2_load_shader(shader->background, gfx_gles2_quad_attr_bindings);
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

    strlist_for_each (res->textures, i, tex_filename)
    {
        char     uniform_name[] = "sTexX";
        stbi_uc* img_data =
            stbi_load(tex_filename, &img_width, &img_height, &img_channels, 3);
        if (img_data == NULL)
        {
            log_warn(
                "Failed to load image \"%s\"\n",
                strlist_cstr(res->textures, 0));
            continue;
        }

        glGenTextures(1, &bg->tex[i]);
        gfx_track_tex(bg->tex[i]);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, bg->tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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

        stbi_image_free(img_data);

        uniform_name[4] = '0' + i;
        bg->sTex[i] =
            gfx_gles2_get_uniform_location_and_warn(bg->program, uniform_name);
    }

    return 0;
}

void gfx_gles2_background_unload(struct gfx_background* bg)
{
    int i;

    if (bg->program != 0)
    {
        gfx_untrack_shader(bg->program);
        glDeleteProgram(bg->program);
        bg->program = 0;
    }

    for (i = 0; i != MAX_TEXTURE_SAMPLERS; ++i)
        if (bg->tex[i] != INVALID_HANDLE)
        {
            gfx_untrack_tex(bg->tex[i]);
            glDeleteTextures(1, &bg->tex[i]);
            bg->tex[i] = INVALID_HANDLE;
            bg->sTex[i] = INVALID_UNIFORM_LOCATION;
        }
}

void gfx_gles2_background_draw(
    const struct world*        world,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar,
    int                        shadow_map_size_factor)
{
    int i;

    gfx_gles2_quad_mesh_prepare_draw(&gfx->quad_mesh);
    glUseProgram(gfx->background.program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gfx->background.texShadow);
    glUniform1i(gfx->background.sShadow, 0);

    for (i = 0; i != MAX_TEXTURE_SAMPLERS; ++i)
        if (gfx->background.tex[i] != INVALID_HANDLE)
        {
            glActiveTexture(GL_TEXTURE0 + 1 + i);
            glBindTexture(GL_TEXTURE_2D, gfx->background.tex[i]);
            glUniform1i(gfx->background.sTex[i], 1 + i);
        }

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

    gfx_gles2_quad_mesh_draw();

    glUseProgram(0);
    gfx_gles2_quad_mesh_end_draw();
}
