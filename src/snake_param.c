#include "clither/snake_param.h"

/* ------------------------------------------------------------------------- */
void snake_param_init(struct snake_param* param)
{
    /* Food count influences base stats */
    param->food_eaten = 40;

    /* Base stats of snake's movement */
    param->base_stats.turn_speed = make_qa2(1, 16);
    param->base_stats.min_speed = make_qw2(1, 96);
    param->base_stats.max_speed = make_qw2(1, 48);
    param->base_stats.boost_speed = make_qw2(1, 16);
    param->base_stats.acceleration = 8;

    param->upgrades.boost = 0;
    param->upgrades.projectile = 0;
    param->upgrades.turn = 0;
    param->upgrades.armor = 0;
    param->upgrades.gather = 0;

    snake_param_update(param, param->upgrades, param->food_eaten);
}

/* ------------------------------------------------------------------------- */
void snake_param_update(
    struct snake_param*   param,
    struct snake_upgrades upgrades,
    uint32_t              food_eaten)
{
    param->upgrades = upgrades;
    param->food_eaten = food_eaten;

    {
        int degrade_scale = 1024;
        qa  degrade_fac = make_qa2(
            degrade_scale,
            (int)food_eaten > degrade_scale ? (int)food_eaten : degrade_scale);
        qa modified = qa_mul(param->base_stats.turn_speed, degrade_fac);
        qa upgrade = make_qa(param->upgrades.turn);
        qa min_turn_speed = make_qa2(1, 64);
        if (modified < min_turn_speed)
            modified = min_turn_speed;
        param->cached_stats.turn_speed = qa_add(modified, upgrade);
    }

    param->cached_stats.min_speed = param->base_stats.min_speed;
    param->cached_stats.max_speed = param->base_stats.max_speed;
    param->cached_stats.boost_speed = param->base_stats.boost_speed;

    param->cached_stats.scale
        = qw_add(make_qw(1), make_qw2(food_eaten, 1024 * 4));
    param->cached_stats.length = make_qw2(food_eaten, 128);

    param->cached_stats.acceleration = param->base_stats.acceleration;
}
