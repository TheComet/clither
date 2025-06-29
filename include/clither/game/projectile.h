#pragma once

#include "clither/game/q.h"
#include "clither/util/bmap.h"
#include "clither/util/morton.h"

struct projectile
{
    struct qwpos dir;
    int16_t      life;
};

BMAP_DECLARE(projectile_bmap, morton, struct projectile, 16)

int projectile_bmap_add(
    struct projectile_bmap** projectiles,
    struct qwpos             pos,
    struct qwpos             dir,
    int16_t                  life);

int projectile_bmap_for_each_in_bb(
    struct projectile_bmap* projectiles,
    struct qwaabb           bb,
    int (*callback)(morton morton, struct projectile* projectile, void* user),
    void* user);
