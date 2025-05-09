#include "clither/game/food.h"
#include "clither/util/morton.h"

BMAP_DEFINE(food_bmap, uint64_t, struct food, 32)

/* ------------------------------------------------------------------------- */
int food_bmap_create_food(
    struct food_bmap** food_bmap, struct qwpos pos, struct qwpos dir)
{
    uint64_t     m = morton_encode_qwpos(pos);
    struct food* new_food;
    switch (food_bmap_emplace_new(food_bmap, m, &new_food))
    {
        case BMAP_OOM: return -1;
        case BMAP_NEW: new_food->dir = dir; new_food->value = 4;
        case BMAP_EXISTS: break;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
struct predicate_bb_ctx
{
    struct qwaabb bb;
    int (*callback)(
        uint64_t morton, struct qwpos pos, struct food* food, void* user);
    void* user;
};
static int predicate_bb(uint64_t morton, struct food* food, void* user)
{
    struct predicate_bb_ctx* ctx = user;
    struct qwpos             pos = morton_decode_qwpos(morton);
    if (qwaabb_test_qwpos(ctx->bb, pos))
        return ctx->callback(morton, pos, food, ctx->user);
    return BMAP_RETAIN;
}
int food_bmap_for_each_in_bb(
    struct food_bmap* food_bmap,
    struct qwaabb     bb,
    int (*callback)(
        uint64_t morton, struct qwpos pos, struct food* food, void* user),
    void* user)
{
    struct predicate_bb_ctx ctx;

    struct qwpos lower_pos = make_qwposqw(bb.x1, bb.y1);
    uint64_t     lower_morton = morton_encode_qwpos(lower_pos);
    int32_t      lower_idx = food_bmap_lower_bound(food_bmap, lower_morton);

    struct qwpos upper_pos = make_qwposqw(bb.x2, bb.y2);
    uint64_t     upper_morton = morton_encode_qwpos(upper_pos);
    int32_t      upper_idx = food_bmap_lower_bound(food_bmap, upper_morton);

    /* bb is inclusive, but upper_idx is exclusve */
    upper_idx++;
    if (upper_idx > bmap_count(food_bmap))
        upper_idx = bmap_count(food_bmap);

    ctx.bb = bb;
    ctx.callback = callback;
    ctx.user = user;
    return food_bmap_retain_range(
        food_bmap, lower_idx, upper_idx, predicate_bb, &ctx);
}

/* ------------------------------------------------------------------------- */
struct predicate_radius_ctx
{
    struct qwpos pos;
    qw           radius_sq;
    int (*callback)(uint64_t morton, struct food* food, void* user);
    void* user;
};
static int predicate_radius(uint64_t morton, struct food* food, void* user)
{
    struct predicate_radius_ctx* ctx = user;
    struct qwpos                 pos = morton_decode_qwpos(morton);
    qw                           dx = qw_sub(ctx->pos.x, pos.x);
    qw                           dy = qw_sub(ctx->pos.y, pos.y);
    qw dist_sq = qw_add(qw_mul(dx, dx), qw_mul(dy, dy));
    if (dist_sq <= ctx->radius_sq)
        return ctx->callback(morton, food, ctx->user);
    return BMAP_RETAIN;
}
int food_bmap_for_each_in_radius(
    struct food_bmap* food_bmap,
    struct qwpos      pos,
    qw                radius,
    int (*callback)(uint64_t morton, struct food* food, void* user),
    void* user)
{
    struct predicate_radius_ctx ctx;

    struct qwpos lower_pos =
        make_qwposqw(qw_sub(pos.x, radius), qw_sub(pos.y, radius));
    uint64_t lower_morton = morton_encode_qwpos(lower_pos);
    int32_t  lower_idx = food_bmap_lower_bound(food_bmap, lower_morton);

    struct qwpos upper_pos =
        make_qwposqw(qw_add(pos.x, radius), qw_add(pos.y, radius));
    uint64_t upper_morton = morton_encode_qwpos(upper_pos);
    int32_t  upper_idx = food_bmap_lower_bound(food_bmap, upper_morton);

    ctx.pos = pos;
    ctx.radius_sq = qw_mul(radius, radius);
    ctx.callback = callback;
    ctx.user = user;
    return food_bmap_retain_range(
        food_bmap, lower_idx, upper_idx, predicate_radius, &ctx);
}
