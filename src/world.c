#include "clither/log.h"
#include "clither/q.h"
#include "clither/snake.h"
#include "clither/snake_bmap.h"
#include "clither/str.h"
#include "clither/world.h"
#include <stddef.h>

/* ------------------------------------------------------------------------- */
void world_init(struct world* world)
{
    snake_bmap_init(&world->snakes);

    world->inner_radius = make_qw(20);
    world->ring_start = make_qw(40);
    world->ring_end = make_qw(64);
}

/* ------------------------------------------------------------------------- */
void world_deinit(struct world* world)
{
    int16_t       idx;
    uint16_t      uid;
    struct snake* snake;
    bmap_for_each (world->snakes, idx, uid, snake)
    {
        (void)uid;
        snake_deinit(snake);
    }
    snake_bmap_deinit(world->snakes);
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
    /* Snake ID 0 is reserved to mean "invalid" */
    uint16_t i, snake_id = 1;
    for (i = 0; i != bmap_count(world->snakes); ++i)
    {
        if (i + 1 != (int)snake_id)
            break;
        snake_id++;
    }

    world_create_snake(world, snake_id, make_qwposi(0, 0), username);

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
void world_step(
    struct world* world, uint16_t frame_number, uint8_t sim_tick_rate)
{
}
