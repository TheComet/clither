#pragma once

#include "clither/game/q.h"
#include "clither/util/bmap.h"
#include "clither/util/morton.h"

struct food
{
    /* Direction vector (normalized). Used for rotating the sprite */
    struct qwpos dir;
    /* How much the snake grows when it eats this food */
    uint8_t value;
};

BMAP_DECLARE(food_bmap, morton, struct food, 32)

int food_bmap_create_food(
    struct food_bmap** food_bmap, struct qwpos pos, struct qwpos dir);

int food_bmap_for_each_in_bb(
    struct food_bmap* food_bmap,
    struct qwaabb     bb,
    int (*callback)(morton morton, struct food* food, void* user),
    void* user);

int food_bmap_for_each_in_radius(
    struct food_bmap* food_bmap,
    struct qwpos      pos,
    qw                radius,
    int (*callback)(morton morton, struct food* food, void* user),
    void* user);
