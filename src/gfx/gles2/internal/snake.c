#include "./gfx.h"
#include "./snake.h"
#include "./sprite.h"
#include "./sprite_shadow.h"
#include "clither/game/bezier_knot_rb.h"
#include "clither/game/bezier_segment_rb.h"
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
    struct bezier_sample sample;

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
    for (i = 0,
        bezier_sample_begin(
             &sample,
             snake->data.segments,
             make_qw2(1, 6),
             snake_length(&snake->param));
         !bezier_sample_end(&sample);
         bezier_sample_next(&sample))
    {
        const struct bezier_segment* segment = bezier_sample_segment(&sample);
        if (i++ == 0)
            continue; /* Skip body part at head position */
        gfx_gles2_sprite_shadow_update_uniforms(
            &gfx->sprite_shadow_mat,
            &gfx->body0_base,
            sample.pos,
            bezier_tangent(segment, sample.t),
            snake_scale(&snake->param),
            camera);
        gfx_gles2_sprite_shadow_draw();
    }

    /* head */
    if (rb_count(snake->data.segments) > 0)
    {
        const struct bezier_segment* segment =
            rb_peek_write(snake->data.segments);
        gfx_gles2_sprite_shadow_bind_textures(&gfx->head0_base);
        gfx_gles2_sprite_shadow_update_uniforms(
            &gfx->sprite_shadow_mat,
            &gfx->head0_base,
            segment->p[0],
            bezier_tangent(segment, 0),
            snake_scale(&snake->param),
            camera);
        gfx_gles2_sprite_shadow_draw();

        gfx_gles2_sprite_shadow_bind_textures(&gfx->head0_gather);
        gfx_gles2_sprite_shadow_draw();
    }

    gfx_gles2_sprite_shadow_end_draw(gfx->width, gfx->height);
}

void gfx_gles2_draw_snake_spine(
    const struct snake*        snake,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar)
{
    int i;

    gfx_gles2_spine_prepare_draw(&gfx->spine);

    for (i = rb_count(snake->data.knots) - 2; i >= 0; --i)
        gfx_gles2_spine_draw(
            &gfx->spine,
            rb_peek(snake->data.knots, i + 1),
            rb_peek(snake->data.knots, i + 0),
            snake_scale(&snake->param),
            camera,
            ar);
    gfx_gles2_spine_end_draw();
}

void gfx_gles2_draw_snake(
    const struct snake*        snake,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar)
{
    int32_t              i;
    struct bezier_sample sample;

    gfx_gles2_sprite_prepare_draw(&gfx->quad_mesh, &gfx->sprite_mat, ar);

    for (i = 0,
        bezier_sample_begin(
             &sample,
             snake->data.segments,
             make_qw2(1, 4),
             snake_length(&snake->param));
         !bezier_sample_end(&sample);
         i++, bezier_sample_next(&sample))
    {
        const struct bezier_segment* segment = bezier_sample_segment(&sample);
        gfx_gles2_sprite_update_uniforms(
            &gfx->sprite_mat,
            &gfx->body0_base,
            sample.pos,
            bezier_tangent(segment, sample.t),
            snake_scale(&snake->param),
            camera);

        if (i == 0)
        {
            gfx_gles2_sprite_bind_textures(&gfx->head0_base);
            gfx_gles2_sprite_draw();
            gfx_gles2_sprite_bind_textures(&gfx->head0_gather);
            gfx_gles2_sprite_draw();
        }
        else
        {
            gfx_gles2_sprite_bind_textures(&gfx->body0_base);
            gfx_gles2_sprite_draw();
        }
    }

    gfx_gles2_sprite_end_draw();
}
