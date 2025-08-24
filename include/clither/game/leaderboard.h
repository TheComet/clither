#pragma once

#include <stdint.h>

struct str;
struct world;

enum
{
    LEADERBOARD_ROW_COUNT = 10
};

struct leaderboard_entry
{
    struct str* username;
    uint32_t    score;
};

struct leaderboard
{
    struct leaderboard_entry rows[LEADERBOARD_ROW_COUNT];
};

void leaderboard_init(struct leaderboard* board);
void leaderboard_deinit(struct leaderboard* board);
int  leaderboard_update(struct leaderboard* board, const struct world* world);
int  leaderboard_set(
     struct leaderboard* board, int idx, const char* username, uint32_t score);
void leaderboard_clear(struct leaderboard* board, int idx);
