#pragma once

#include "clither/config.h"
#include "clither/q.h"
#include <stdint.h>

C_BEGIN

struct cs_vector;
struct cs_rb;

struct bezier_point
{
    struct qwpos pos;
    struct qwpos dir;
};

/*!
 * \brief Represents a knot in the bezier curve.
 * 
 * The start of one bezier curve shares the angle with the end of the next
 * bezier curve. The vectors [cos(a), sin(a)] of each knot always point
 * "backwards", i.e. away from the head.
 * 
 * len_backwards and len_forwards store the distance to the intermediate
 * control points. Divide by 255 to convert it into a qw type.
 */
struct bezier_handle
{
    struct qwpos pos;
    qa angle;
    uint16_t id;
    uint8_t len_backwards, len_forwards;
};

void
bezier_handle_init(struct bezier_handle* bh, struct qwpos pos, qa angle);

/*!
 * \brief Performs a constrained least squares fit on the input data points to
 * generate a 3rd degree bezier curve that fits the data.
 * \param[in] head The head bezier handle will be positioned and rotated to align
 * with the data. head->len_backwards will also be updated.
 * \param[in] tail The tail bezier handle will only have its tail->len_forwards
 * updated. The angle and position are assumed to be correct from the previous
 * bezier segment.
 * \param[in] points A list of qwpos2 points to fit the data to.
 * \return Returns the least squared error of the fit.
 */
double
bezier_fit_head(
        struct bezier_handle* head,
        struct bezier_handle* tail,
        const struct cs_vector* points);

/*!
 * \brief Adjusts all bezier handles in a way to cause the snake to "squeeze"
 * over time, i.e. tight circles become tighter over time.
 * \param[in,out] bezier_handles A list of all bezier handles forming the curve.
 * \param[in] sim_tick_rate Simulation tick rate.
 */
void
bezier_squeeze_step(
    struct cs_rb* bezier_handles,
    int sim_tick_rate);

void
bezier_squeeze_n_recent_step(
    struct cs_rb* bezier_handles,
    int n,
    int sim_tick_rate);

/*!
 * \brief Samples the curve at constant intervals and stores each position
 * into the bezier_points structure.
 * \param[out] bezier_points The resulting points are written to this array.
 * The array is cleared every time, so no need to do that before calling.
 * \param[in] bezier_handles The list of bezier handles comprising the curve.
 * \param[in] spacing The distance between each sampled point on the curve,
 * in world space.
 * \param[in] snake_length The required total length of the snake, in world
 * space.
 */
int
bezier_calc_equidistant_points(
    struct cs_vector* bezier_points,
    const struct cs_rb* bezier_handles,
    qw spacing,
    qw snake_length);

/*!
 * This function tries to determine how many curve segments are unnecessary at
 * the end of the snake. On the client-side, this is calculated using the
 * snake's acknowledged head position, which will be located somewhere on the
 * curve but not necessarily at the front of the curve.
 */
int
bezier_calc_unused_segments(
    const struct cs_rb* bezier_handles,
    struct qwpos head_pos,
    qw spacing,
    qw snake_length);

static inline int
bezier_handles_equal(const struct bezier_handle* a, const struct bezier_handle* b)
{
    return
        a->pos.x == b->pos.x &&
        a->pos.y == b->pos.y &&
        a->angle == b->angle &&
        a->len_backwards == b->len_backwards &&
        a->len_forwards == b->len_forwards;
}

static inline int
bezier_handles_equal_pos(const struct bezier_handle* a, const struct bezier_handle* b)
{
    return
        a->pos.x == b->pos.x &&
        a->pos.y == b->pos.y;
}

C_END
