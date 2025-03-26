#include "clither/food.h"
#include "clither/hash.h"
#include "clither/log.h"
#include "clither/q.h"
#include "clither/settings.h"
#include "clither/snake.h"
#include "clither/snake_bmap.h"
#include "clither/str.h"
#include "clither/world.h"
#include <stddef.h>

/* ------------------------------------------------------------------------- */
void world_init(struct world* world, const struct settings_world* settings)
{
    snake_bmap_init(&world->snakes);
    food_grid_init(&world->food_grid);

    world->food_count = settings->food_count;
    world->food_rng = 1;
    world->inner_radius = make_qw(settings->inner_radius);
    world->ring_start = make_qw(settings->ring_start);
    world->ring_end = make_qw(settings->ring_end);
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
    food_grid_deinit(&world->food_grid);
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
    snake_init(snake, spawn_pos, username);

    log_info(
        "Creating snake id: %d, pos: [%.2f,%.2f], username: \"%s\"\n",
        snake_id,
        qw_to_float(snake->head.pos.x),
        qw_to_float(snake->head.pos.y),
        username);

    return snake;
}

/* ------------------------------------------------------------------------- */
uint16_t world_spawn_snake(struct world* world, const char* username)
{
    struct qwpos spawn = make_qwposi(0, 0);

    /* Snake ID 0 is reserved to mean "invalid" */
    uint16_t snake_id = world->next_free_snake_id++;
    if (world->next_free_snake_id == 0)
        world->next_free_snake_id++;

    if (world_create_snake(world, snake_id, spawn, username) == NULL)
        return 0;

    return snake_id;
}

/* ------------------------------------------------------------------------- */
void world_remove_snake(struct world* world, uint16_t snake_id)
{
    struct snake* snake = snake_bmap_find(world->snakes, snake_id);
    if (snake == NULL)
    {
        log_warn("Tried removing snake %d, but it doesn't exist\n", snake_id);
        return;
    }

    log_info(
        "Removing snake %d with username \"%s\"\n",
        snake_id,
        str_cstr(snake->data.name));
    snake_deinit(snake);
    snake_bmap_erase(world->snakes, snake_id);
}

/* ------------------------------------------------------------------------- */
static uint64_t food_rng(struct world* w)
{
    return w->food_rng +=
           hash32_jenkins_oaat(&w->food_rng, sizeof(w->food_rng));
}
int world_respawn_food(struct world* w)
{
    while (food_grid_food_count(&w->food_grid) < w->food_count)
    {
        struct qwpos pos;
        qa           dir = (qa)(food_rng(w));
        qa           phi = (qa)(food_rng(w));
        qw           r = (qw)(food_rng(w) & 0x7FFFFFFF);
        r = qw_rescale(r, w->inner_radius, 1 << 31);
        pos = make_qwposqw(qw_mul(qa_cos(phi), r), qw_mul(qa_sin(phi), r));
        if (food_grid_add_food(&w->food_grid, pos, dir) != 0)
            return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
int world_step(
    struct world* world, uint16_t frame_number, uint8_t sim_tick_rate)
{
    return 0;
}
