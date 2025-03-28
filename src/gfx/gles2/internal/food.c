#include "./gfx.h"
#include "clither/bmap.h"
#include "clither/food.h"
#include "clither/morton.h"

void gfx_gles2_draw_food_shadows(
    const struct food_grid*    food_grid,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar,
    int                        shadow_map_size_factor)
{
    int32_t            idx;
    uint64_t           morton;
    const struct food* food;

    gfx_gles2_sprite_shadow_prepare_draw(
        &gfx->background,
        &gfx->quad_mesh,
        &gfx->sprite_shadow_mat,
        ar,
        gfx->width,
        gfx->height,
        shadow_map_size_factor);
    gfx_gles2_sprite_shadow_bind_textures(&gfx->food);
    bmap_for_each (food_grid->morton, idx, morton, food)
    {
        gfx_gles2_sprite_shadow_update_uniforms(
            &gfx->sprite_shadow_mat,
            &gfx->food,
            morton_decode_qwpos(morton),
            food->dir,
            make_qw(1),
            camera);
        gfx_gles2_sprite_shadow_draw();
    }

    gfx_gles2_sprite_shadow_end_draw(gfx->width, gfx->height);
}

void gfx_gles2_draw_food(
    const struct food_grid*    food_grid,
    const struct gfx*          gfx,
    const struct camera*       camera,
    const struct aspect_ratio* ar)
{
    int32_t            idx;
    uint64_t           morton;
    const struct food* food;

    gfx_gles2_sprite_prepare_draw(&gfx->quad_mesh, &gfx->sprite_mat, ar);
    gfx_gles2_sprite_bind_textures(&gfx->food);
    bmap_for_each (food_grid->morton, idx, morton, food)
    {
        gfx_gles2_sprite_update_uniforms(
            &gfx->sprite_mat,
            &gfx->food,
            morton_decode_qwpos(morton),
            food->dir,
            make_qw(1),
            camera);
        gfx_gles2_sprite_draw();
    }

    gfx_gles2_sprite_end_draw();
}
