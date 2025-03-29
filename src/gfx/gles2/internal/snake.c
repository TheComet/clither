#include "./gfx.h"
#include "./snake.h"
#include "./sprite.h"
#include "./sprite_shadow.h"
#include "clither/game/bezier_point_vec.h"
#include "clither/game/snake.h"
#include "clither/util/vec.h"

void gfx_gles2_draw_snake_shadow(
    const struct snake*        snake,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar,
    int                        shadow_map_size_factor)
{
    int32_t              i;
    struct bezier_point* bp;

    gfx_gles2_sprite_shadow_prepare_draw(
        &gfx->background,
        &gfx->quad_mesh,
        &gfx->sprite_shadow_mat,
        ar,
        gfx->width,
        gfx->height,
        shadow_map_size_factor);

    /* body parts */
    gfx_gles2_sprite_shadow_bind_textures(&gfx->body0_base);
    vec_enumerate_r(snake->data.bezier_points, i, bp)
    {
        /* Skip body part at head position */
        if (i == 0)
            break;
        gfx_gles2_sprite_shadow_update_uniforms(
            &gfx->sprite_shadow_mat,
            &gfx->body0_base,
            bp->pos,
            bp->dir,
            snake_scale(&snake->param),
            camera);
        gfx_gles2_sprite_shadow_draw();
    }

    /* head */
    if (vec_count(snake->data.bezier_points) > 0)
    {
        bp = vec_first(snake->data.bezier_points);
        gfx_gles2_sprite_shadow_bind_textures(&gfx->head0_base);
        gfx_gles2_sprite_shadow_update_uniforms(
            &gfx->sprite_shadow_mat,
            &gfx->head0_base,
            bp->pos,
            bp->dir,
            snake_scale(&snake->param),
            camera);
        gfx_gles2_sprite_shadow_draw();

        gfx_gles2_sprite_shadow_bind_textures(&gfx->head0_gather);
        gfx_gles2_sprite_shadow_draw();
    }

    gfx_gles2_sprite_shadow_end_draw(gfx->width, gfx->height);
}

void gfx_gles2_draw_snake(
    const struct snake*        snake,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar)
{
    int32_t              i;
    struct bezier_point* bp;

    gfx_gles2_sprite_prepare_draw(&gfx->quad_mesh, &gfx->sprite_mat, ar);

    /* body parts */
    gfx_gles2_sprite_bind_textures(&gfx->body0_base);
    vec_enumerate_r(snake->data.bezier_points, i, bp)
    {
        /* Skip body part at head position */
        if (i == 0)
            break;
        gfx_gles2_sprite_update_uniforms(
            &gfx->sprite_mat,
            &gfx->body0_base,
            bp->pos,
            bp->dir,
            snake_scale(&snake->param),
            camera);
        gfx_gles2_sprite_draw();
    }

    /* head */
    if (vec_count(snake->data.bezier_points) > 0)
    {
        bp = vec_first(snake->data.bezier_points);
        gfx_gles2_sprite_bind_textures(&gfx->head0_base);
        gfx_gles2_sprite_update_uniforms(
            &gfx->sprite_mat,
            &gfx->head0_base,
            bp->pos,
            bp->dir,
            snake_scale(&snake->param),
            camera);
        gfx_gles2_sprite_draw();

        gfx_gles2_sprite_bind_textures(&gfx->head0_gather);
        gfx_gles2_sprite_draw();
    }

    gfx_gles2_sprite_end_draw();
}
