#include "./gfx.h"
#include "./snake.h"
#include "./sprite.h"
#include "./sprite_shadow.h"
#include "clither/game/bezier_knot_rb.h"
#include "clither/game/bezier_segment_rb.h"
#include "clither/game/qwpos_vec.h"
#include "clither/game/resource_pack.h"
#include "clither/game/snake.h"
#include "clither/util/strlist.h"

VEC_DEFINE(gfx_part_sample_vec, struct gfx_part_sample, 32)
VEC_DEFINE(gfx_sprite_tex_vec, struct gfx_sprite_tex, 8)

/* ------------------------------------------------------------------------- */
int gfx_gles2_snake_init(struct gfx_snake* gfx)
{
    gfx_gles2_sprite_tex_init(&gfx->head_base);
    gfx_gles2_sprite_tex_init(&gfx->head_gather);
    gfx_sprite_tex_vec_init(&gfx->body_base);
    gfx_gles2_sprite_tex_init(&gfx->tail_base);
    gfx_gles2_spine_init(&gfx->spine);

    gfx_part_sample_vec_init(&gfx->part_samples);

    return 0;
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_snake_deinit(struct gfx_snake* gfx)
{
    struct gfx_sprite_tex* tex;

    gfx_part_sample_vec_deinit(gfx->part_samples);

    gfx_gles2_spine_deinit(&gfx->spine);
    gfx_gles2_sprite_tex_deinit(&gfx->tail_base);

    vec_for_each (gfx->body_base, tex)
        gfx_gles2_sprite_tex_deinit(tex);
    gfx_sprite_tex_vec_deinit(gfx->body_base);

    gfx_gles2_sprite_tex_deinit(&gfx->head_gather);
    gfx_gles2_sprite_tex_deinit(&gfx->head_base);
}

/* ------------------------------------------------------------------------- */
int gfx_gles2_snake_load(
    struct gfx_snake*             snake,
    const struct resource_snake*  res,
    const struct resource_pack*   pack,
    const struct resource_shader* shader)
{
    int                     i;
    struct resource_sprite* sprite;
    struct resource_spine*  spine;
    struct strview          sprite_name;

    sprite =
        resource_sprite_hmap_find(pack->sprites, str_view(res->head_sprite));
    if (sprite != NULL)
    {
        gfx_gles2_sprite_tex_load(
            &snake->head_base, &sprite->layer[RESOURCE_LAYER_BASE]);
        gfx_gles2_sprite_tex_load(
            &snake->head_gather, &sprite->layer[RESOURCE_LAYER_GATHER]);
    }

    strlist_for_each (res->body_sprites, i, sprite_name)
    {
        sprite = resource_sprite_hmap_find(pack->sprites, sprite_name);
        if (sprite != NULL)
        {
            struct gfx_sprite_tex* tex =
                gfx_sprite_tex_vec_emplace(&snake->body_base);
            if (tex == NULL)
                return -1;
            gfx_gles2_sprite_tex_init(tex);
            gfx_gles2_sprite_tex_load(tex, &sprite->layer[RESOURCE_LAYER_BASE]);
        }
    }

    sprite =
        resource_sprite_hmap_find(pack->sprites, str_view(res->tail_sprite));
    if (sprite != NULL)
    {
        gfx_gles2_sprite_tex_load(
            &snake->tail_base, &sprite->layer[RESOURCE_LAYER_BASE]);
    }

    spine = resource_spine_hmap_find(pack->spines, str_view(res->spine));
    if (spine != NULL)
    {
        gfx_gles2_spine_load(&snake->spine, spine, shader);
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_snake_unload(struct gfx_snake* gfx)
{
    struct gfx_sprite_tex* tex;

    gfx_gles2_spine_unload(&gfx->spine);
    gfx_gles2_sprite_tex_unload(&gfx->tail_base);

    vec_for_each (gfx->body_base, tex)
    {
        gfx_gles2_sprite_tex_unload(tex);
        gfx_gles2_sprite_tex_deinit(tex);
    }
    gfx_sprite_tex_vec_clear_compact(&gfx->body_base);

    gfx_gles2_sprite_tex_unload(&gfx->head_gather);
    gfx_gles2_sprite_tex_unload(&gfx->head_base);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_snake_step_anim(struct gfx_snake* gfx, int sim_tick_rate)
{
    struct gfx_sprite_tex* tex;

    gfx_gles2_sprite_step_anim(&gfx->head_base, sim_tick_rate);
    gfx_gles2_sprite_step_anim(&gfx->head_gather, sim_tick_rate);
    vec_for_each (gfx->body_base, tex)
        gfx_gles2_sprite_step_anim(tex, sim_tick_rate);
    gfx_gles2_sprite_step_anim(&gfx->tail_base, sim_tick_rate);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_draw_snake_shadow(
    struct gfx_snake*          gfx_snake,
    const struct gfx*          gfx,
    const struct snake*        snake,
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
    gfx_gles2_sprite_shadow_bind_textures(vec_first(gfx_snake->body_base));
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
            vec_first(gfx_snake->body_base),
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
        gfx_gles2_sprite_shadow_bind_textures(&gfx_snake->head_base);
        gfx_gles2_sprite_shadow_update_uniforms(
            &gfx->sprite_shadow_mat,
            &gfx_snake->head_base,
            segment->p[0],
            bezier_tangent(segment, 0),
            snake_scale(&snake->param),
            camera);
        gfx_gles2_sprite_shadow_draw();

        gfx_gles2_sprite_shadow_bind_textures(&gfx_snake->head_gather);
        gfx_gles2_sprite_shadow_draw();
    }

    gfx_gles2_sprite_shadow_end_draw(gfx->width, gfx->height);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_draw_snake(
    struct gfx_snake*          gfx_snake,
    const struct gfx*          gfx,
    const struct snake*        snake,
    const struct camera*       camera,
    const struct aspect_ratio* ar)
{
    int32_t                 i;
    int                     oversample_factor;
    struct bezier_sample    sample;
    struct gfx_part_sample* part_sample;
    qw                      sample_spacing;

    /* If the spacing is really far apart, then we undersample the bezier curve
     * and the accumulated distance becomes inaccurate. This compensates for
     * that */
    sample_spacing = qw_mul(
        snake_scale(&snake->param), make_qw(snake->param.cosmetic.part_spacing));
    oversample_factor = 1;
    while (sample_spacing > make_qw(0.1))
    {
        sample_spacing = qw_div(sample_spacing, make_qw(2));
        oversample_factor *= 2;
    }

    gfx_part_sample_vec_clear(gfx_snake->part_samples);
    for (i = 0,
        bezier_sample_begin(
             &sample,
             snake->data.segments,
             sample_spacing,
             snake_length(&snake->param));
         !bezier_sample_end(&sample);
         i++, bezier_sample_next(&sample))
    {
        if (i % oversample_factor == 0)
        {
            struct gfx_part_sample* ps =
                gfx_part_sample_vec_emplace(&gfx_snake->part_samples);
            ps->pos = sample.pos;
            ps->dir = bezier_tangent(bezier_sample_segment(&sample), sample.t);
            ps->length = q16_16_to_qw(sample.total_spacing);
        }
    }

    gfx_gles2_spine_prepare_draw(&gfx_snake->spine);
    gfx_gles2_spine_draw(
        &gfx_snake->spine,
        snake->data.segments,
        snake_scale(&snake->param),
        vec_count(gfx_snake->part_samples) > 0
            ? vec_last(gfx_snake->part_samples)->length
            : make_qw(0),
        snake->param.cosmetic.spine_width,
        camera,
        ar);
    gfx_gles2_spine_end_draw();

    gfx_gles2_sprite_prepare_draw(&gfx->quad_mesh, &gfx->sprite_mat, ar);
    vec_enumerate (gfx_snake->part_samples, i, part_sample)
    {
        /* head */
        if (i == 0)
        {
            if (gfx_gles2_sprite_update_uniforms(
                    &gfx->sprite_mat,
                    &gfx_snake->head_base,
                    part_sample->pos,
                    part_sample->dir,
                    qw_to_float(snake_scale(&snake->param)) *
                        snake->param.cosmetic.head_scale,
                    camera) &&
                gfx_gles2_sprite_bind_textures(&gfx_snake->head_base))
            {
                gfx_gles2_sprite_draw();
            }

            if (gfx_gles2_sprite_update_uniforms(
                    &gfx->sprite_mat,
                    &gfx_snake->head_base,
                    part_sample->pos,
                    part_sample->dir,
                    qw_to_float(snake_scale(&snake->param)) *
                        snake->param.cosmetic.head_scale,
                    camera) &&
                gfx_gles2_sprite_bind_textures(&gfx_snake->head_gather))
            {
                gfx_gles2_sprite_draw();
            }
        }
        else if (i == vec_count(gfx_snake->part_samples) - 1) /* tail */
        {
            if (gfx_gles2_sprite_update_uniforms(
                    &gfx->sprite_mat,
                    &gfx_snake->tail_base,
                    part_sample->pos,
                    part_sample->dir,
                    qw_to_float(snake_scale(&snake->param)) *
                        snake->param.cosmetic.tail_scale,
                    camera) &&
                gfx_gles2_sprite_bind_textures(&gfx_snake->tail_base))
            {
                gfx_gles2_sprite_draw();
            }
        }
        else if (vec_count(gfx_snake->body_base) > 0) /* body */
        {
            const struct gfx_sprite_tex* tex = vec_get(
                gfx_snake->body_base,
                (i - 1) % vec_count(gfx_snake->body_base));
            if (gfx_gles2_sprite_update_uniforms(
                    &gfx->sprite_mat,
                    tex,
                    part_sample->pos,
                    part_sample->dir,
                    qw_to_float(snake_scale(&snake->param)) *
                        snake->param.cosmetic.body_scale,
                    camera) &&
                gfx_gles2_sprite_bind_textures(tex))
            {
                gfx_gles2_sprite_draw();
            }
        }
    }

    gfx_gles2_sprite_end_draw();
}
