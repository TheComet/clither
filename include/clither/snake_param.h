#pragma once

#include "clither/q.h"
#include <stdint.h>

struct snake_upgrades
{
    unsigned boost      : 2;
    unsigned projectile : 2;
    unsigned turn       : 1;
    unsigned armor      : 1;
    unsigned gather     : 1;
};

struct snake_param
{
    uint32_t food_eaten;

    struct {
        qa turn_speed;
        qw min_speed;
        qw max_speed;
        qw boost_speed;
        uint8_t acceleration;
    } base_stats;

    struct {
        qa turn_speed;
        qw min_speed;
        qw max_speed;
        qw boost_speed;
        qw scale;
        qw length;
        uint8_t acceleration;
    } cached_stats;

    struct snake_upgrades upgrades;
};

void
snake_param_init(struct snake_param* param);

void
snake_param_update(struct snake_param* param, struct snake_upgrades upgrades, uint32_t food_eaten);

/*!
 * \brief Converts the snake's "food_eaten" parameter into a factor for how
 * much to scale the snake.
 *
 * This factor determines the spacing in between each sprite, the scale
 * of each sprite, and controls the camera's zoom.
 */
#define snake_scale(param) ((param)->cached_stats.scale)

/*!
 * \brief Converts the snake's "food_eaten" parameter into an expected total
 * length (in world space) of the snake.
 *
 * This is used when sampling the curve and is also used as a metric for when
 * to remove curve segments from the ring buffer.
 */
#define snake_length(param) ((param)->cached_stats.length)

#define snake_turn_speed(param) ((param)->cached_stats.turn_speed)

#define snake_boost_speed(param) ((param)->cached_stats.boost_speed)

#define snake_min_speed(apram) ((param)->cached_stats.min_speed)

#define snake_max_speed(param) ((param)->cached_stats.max_speed)

#define snake_acceleration(param) ((param)->cached_stats.acceleration)

