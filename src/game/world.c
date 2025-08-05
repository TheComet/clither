#include "clither/game/food.h"
#include "clither/game/q.h"
#include "clither/game/settings.h"
#include "clither/game/snake.h"
#include "clither/game/snake_bmap.h"
#include "clither/game/world.h"
#include "clither/util/hash.h"
#include "clither/util/log.h"
#include "clither/util/str.h"
#include <stddef.h>

/* ------------------------------------------------------------------------- */
static uint64_t rng(struct world* w)
{
    return w->rng += hash32_jenkins_oaat(&w->rng, sizeof(w->rng));
}

/* ------------------------------------------------------------------------- */
void world_init(struct world* world)
{
    snake_bmap_init(&world->snakes);
    food_bmap_init(&world->food_bmap);

    world->food_count = 0;
    world->rng = 1;
    world->inner_radius = 0;
    world->ring_start = 0;
    world->ring_end = 0;
    world->next_free_snake_id = 1;
}

/* ------------------------------------------------------------------------- */
void world_deinit(struct world* world)
{
    int16_t       idx;
    uint16_t      uid;
    struct snake* snake;
    bmap_for_each (world->snakes, idx, uid, snake)
        (void)uid, snake_deinit(snake);
    snake_bmap_deinit(world->snakes);
    food_bmap_deinit(world->food_bmap);
}

/* ------------------------------------------------------------------------- */
void world_update_settings(
    struct world* world, const struct settings_world* settings)
{
    world->food_count = settings->food_count;
    world->inner_radius = make_qw(settings->inner_radius);
    world->ring_start = make_qw(settings->ring_start);
    world->ring_end = make_qw(settings->ring_end);
}

/* ------------------------------------------------------------------------- */
struct snake* world_create_snake(
    struct world* world,
    uint16_t      snake_id,
    struct qwpos  spawn_pos,
    const char*   username)
{
    struct snake* snake;
    if (snake_bmap_emplace_new(&world->snakes, snake_id, &snake) != BMAP_NEW)
        return NULL;
    if (snake_init(snake, spawn_pos, username) != 0)
    {
        snake_bmap_erase(world->snakes, snake_id);
        return NULL;
    }

    return snake;
}

/* ------------------------------------------------------------------------- */
static int
is_position_out_of_visible_range(const struct world* world, struct qwpos pos)
{
    int16_t             idx;
    uint16_t            snake_id;
    const struct snake* snake;
    bmap_for_each (world->snakes, idx, snake_id, snake)
    {
        struct qwpos  range = snake_calculate_visible_range(snake);
        struct qwaabb visible_bb = make_qwaabbqw(
            qw_sub(snake->head.pos.x, range.x),
            qw_sub(snake->head.pos.y, range.y),
            qw_add(snake->head.pos.x, range.x),
            qw_add(snake->head.pos.y, range.y));
        if (qwaabb_test_qwpos(visible_bb, pos))
            return 0;

        (void)idx, (void)snake_id;
    }

    return 1;
}

/* ------------------------------------------------------------------------- */
static struct qwpos find_spawn_position(struct world* world)
{
    struct qwpos spawn;
    int          tries = 32;
    do
    {
        qa phi = (qa)(rng(world));
        qw r = (qw)(rng(world) & 0x7FFFFFFF);
        r = qw_rescale(r, world->inner_radius, 1 << 31);
        spawn = make_qwposqw(qw_mul(qa_cos(phi), r), qw_mul(qa_sin(phi), r));
    } while (!is_position_out_of_visible_range(world, spawn) && --tries);

    return spawn;
}

/* ------------------------------------------------------------------------- */
uint16_t world_spawn_snake(struct world* world, const char* username)
{
    uint16_t     snake_id;
    struct qwpos spawn;

    /* Snake ID 0 is reserved to mean "invalid" */
    snake_id = world->next_free_snake_id++;
    if (world->next_free_snake_id == 0)
        world->next_free_snake_id++;

    spawn = find_spawn_position(world);

    if (world_create_snake(world, snake_id, spawn, username) == NULL)
        return 0;

    return snake_id;
}

/* ------------------------------------------------------------------------- */
void world_remove_snake(struct world* world, uint16_t snake_id)
{
    struct snake* snake = snake_bmap_find(world->snakes, snake_id);
    if (snake == NULL)
        return;

    snake_deinit(snake);
    snake_bmap_erase(world->snakes, snake_id);
}

/* ------------------------------------------------------------------------- */
int world_respawn_food(struct world* w)
{
    int tries = 1024 * 64;
    while (bmap_count(w->food_bmap) < w->food_count && --tries)
    {
        struct qwpos pos, dir;
        qa           a = (qa)(rng(w));
        qa           phi = (qa)(rng(w));
        qw           r = (qw)(rng(w) & 0x7FFFFFFF);
        r = qw_rescale(r, w->inner_radius, 1 << 31);
        pos = make_qwposqw(qw_mul(qa_cos(phi), r), qw_mul(qa_sin(phi), r));
        dir = make_qwposqw(qa_cos(a), qa_sin(a));
        if (food_bmap_create_food(&w->food_bmap, pos, dir) != 0)
            return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
int world_spawn_food_corpse(
    struct world*             w,
    const struct snake_data*  data,
    const struct snake_param* param)
{
    int                  i;
    struct bezier_sample sample;

    for (bezier_sample_begin(
             &sample, data->segments, SNAKE_PART_SPACING, snake_length(param));
         !bezier_sample_end(&sample);
         bezier_sample_next(&sample))
    {
        for (i = 0; i != 10; ++i)
        {
            qw           scale = qw_div(snake_scale(param), make_qw(4));
            qw           dx = qw_rescale(rng(w) & 0x7FFFFFFF, scale, 1 << 31);
            qw           dy = qw_rescale(rng(w) & 0x7FFFFFFF, scale, 1 << 31);
            qa           a = (qa)(rng(w));
            struct qwpos dir = make_qwposqw(qa_cos(a), qa_sin(a));
            struct qwpos pos = make_qwposqw(
                qw_add(sample.pos.x, dx), qw_add(sample.pos.y, dy));
            if (food_bmap_create_food(&w->food_bmap, pos, dir) != 0)
                return -1;
        }
    }

    return 0;
}
