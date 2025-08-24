#include "clither/config.h"
#include "clither/game/leaderboard.h"
#include "clither/game/snake_bmap.h"
#include "clither/game/world.h"
#include "clither/util/str.h"
#include <string.h>

/* ------------------------------------------------------------------------- */
void leaderboard_init(struct leaderboard* board)
{
    memset(board, 0x00, sizeof *board);
}

/* ------------------------------------------------------------------------- */
void leaderboard_deinit(struct leaderboard* board)
{
    int i;
    for (i = 0; i != LEADERBOARD_ROW_COUNT; ++i)
        str_deinit(board->rows[i].username);
}

/* ------------------------------------------------------------------------- */
static void shift_down(struct leaderboard* board, int insert_idx)
{
    int         i;
    struct str* tmp = board->rows[LEADERBOARD_ROW_COUNT - 1].username;
    for (i = LEADERBOARD_ROW_COUNT - 1; i >= insert_idx + 1; --i)
        board->rows[i] = board->rows[i - 1];
    board->rows[insert_idx].username = tmp;
}

/* ------------------------------------------------------------------------- */
int leaderboard_update(struct leaderboard* board, const struct world* world)
{
    int                 i, j;
    uint16_t            snake_id;
    const struct snake* snake;

    for (j = 0; j != LEADERBOARD_ROW_COUNT; ++j)
        board->rows[j].score = 0;

    bmap_for_each (world->snakes, i, snake_id, snake)
    {
        uint32_t          score = snake->param.food_eaten;
        const struct str* name = snake->data.name;
        for (j = 0; j != LEADERBOARD_ROW_COUNT; ++j)
            if (score > board->rows[j].score)
            {
                shift_down(board, j);
                if (leaderboard_set(board, j, str_cstr(name), score) != 0)
                    return -1;

                break;
            }
    }

    (void)snake_id;
    return 0;
}

/* ------------------------------------------------------------------------- */
int leaderboard_set(
    struct leaderboard* board, int idx, const char* username, uint32_t score)
{
    board->rows[idx].score = score;
    return str_set_cstr(&board->rows[idx].username, username);
}

/* ------------------------------------------------------------------------- */
void leaderboard_clear(struct leaderboard* board, int idx)
{
    board->rows[idx].score = 0;
    str_clear(board->rows[idx].username);
}
