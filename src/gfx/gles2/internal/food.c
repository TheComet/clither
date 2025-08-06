#include "./food.h"
#include "./gfx.h"
#include "clither/game/food.h"
#include "clither/game/resource_pack.h"
#include "clither/util/bmap.h"
#include "clither/util/morton.h"

/* ------------------------------------------------------------------------- */
void gfx_gles2_food_init(struct gfx_food* food)
{
    gfx_gles2_sprite_tex_init(&food->tex);
    food->scale = 1.0f;
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_food_deinit(struct gfx_food* food)
{
    gfx_gles2_sprite_tex_deinit(&food->tex);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_food_load(
    struct gfx_food* food, const struct resource_pack* pack)
{
    struct resource_sprite* sprite =
        resource_sprite_hmap_find(pack->sprites, str_view(pack->food.sprite));
    if (sprite != NULL)
    {
        gfx_gles2_sprite_tex_load(
            &food->tex, &sprite->layer[RESOURCE_LAYER_BASE]);
        food->scale = pack->food.scale;
    }
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_food_unload(struct gfx_food* food)
{
    gfx_gles2_sprite_tex_unload(&food->tex);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_food_step_anim(struct gfx_food* food, int sim_tick_rate)
{
    gfx_gles2_sprite_step_anim(&food->tex, sim_tick_rate);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_draw_food_shadows(
    const struct food_bmap*    food_bmap,
    const struct gfx*          gfx,
    const struct gfx_food*     gfx_food,
    const struct camera*       camera,
    const struct aspect_ratio* ar,
    int                        shadow_map_size_factor)
{
    int32_t            idx;
    morton             morton;
    const struct food* food;

    gfx_gles2_sprite_shadow_prepare_draw(
        &gfx->background,
        &gfx->quad_mesh,
        &gfx->sprite_shadow_mat,
        ar,
        gfx->width,
        gfx->height,
        shadow_map_size_factor);
    gfx_gles2_sprite_shadow_bind_textures(&gfx_food->tex);
    bmap_for_each (food_bmap, idx, morton, food)
    {
        gfx_gles2_sprite_shadow_update_uniforms(
            &gfx->sprite_shadow_mat,
            &gfx_food->tex,
            morton_decode_qwpos(morton),
            food->dir,
            gfx_food->scale,
            camera);
        gfx_gles2_sprite_shadow_draw();
    }

    gfx_gles2_sprite_shadow_end_draw(gfx->width, gfx->height);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_draw_food(
    const struct food_bmap*    food_bmap,
    const struct gfx*          gfx,
    const struct gfx_food*     gfx_food,
    const struct camera*       camera,
    const struct aspect_ratio* ar)
{
    int32_t            idx;
    morton             morton;
    const struct food* food;

    gfx_gles2_sprite_prepare_draw(&gfx->quad_mesh, &gfx->sprite_mat, ar);
    gfx_gles2_sprite_bind_textures(&gfx_food->tex);
    bmap_for_each (food_bmap, idx, morton, food)
    {
        gfx_gles2_sprite_update_uniforms(
            &gfx->sprite_mat,
            &gfx_food->tex,
            morton_decode_qwpos(morton),
            food->dir,
            gfx_food->scale,
            camera);
        gfx_gles2_sprite_draw();
    }

    gfx_gles2_sprite_end_draw();
}
