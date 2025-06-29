#include "clither/game/projectile.h"
#include "clither/util/morton.h"

BMAP_DEFINE(projectile_bmap, morton, struct projectile, 16)

/* ------------------------------------------------------------------------- */
int projectile_bmap_add(
    struct projectile_bmap** projectiles,
    struct qwpos             pos,
    struct qwpos             dir,
    int16_t                  life)
{
    morton             m = morton_encode_qwpos(pos);
    struct projectile* projectile;
    switch (projectile_bmap_emplace_new(projectiles, m, &projectile))
    {
        case BMAP_OOM: return -1;
        case BMAP_NEW: projectile->dir = dir; projectile->life = life;
        case BMAP_EXISTS: break;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
struct predicate_bb_ctx
{
    struct qwaabb bb;
    int (*callback)(morton morton, struct projectile* projectile, void* user);
    void* user;
};
static int
predicate_bb(morton morton, struct projectile* projectile, void* user)
{
    struct predicate_bb_ctx* ctx = user;
    struct qwpos             pos = morton_decode_qwpos(morton);
    if (qwaabb_test_qwpos(ctx->bb, pos))
        return ctx->callback(morton, projectile, ctx->user);
    return BMAP_RETAIN;
}
int projectile_grid_for_each_in_bb(
    struct projectile_bmap* projectiles,
    struct qwaabb           bb,
    int (*callback)(morton morton, struct projectile* projectile, void* user),
    void* user)
{
    struct predicate_bb_ctx ctx;

    struct qwpos lower_pos = make_qwposqw(bb.x1, bb.y1);
    morton       lower_morton = morton_encode_qwpos(lower_pos);
    int32_t lower_idx = projectile_bmap_lower_bound(projectiles, lower_morton);

    struct qwpos upper_pos = make_qwposqw(bb.x2, bb.y2);
    morton       upper_morton = morton_encode_qwpos(upper_pos);
    int32_t upper_idx = projectile_bmap_lower_bound(projectiles, upper_morton);

    ctx.bb = bb;
    ctx.callback = callback;
    ctx.user = user;
    return projectile_bmap_retain_range(
        projectiles, lower_idx, upper_idx, predicate_bb, &ctx);
}
