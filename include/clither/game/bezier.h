#pragma once

#include "clither/game/q.h"

struct qwpos_vec;
struct bezier_knot_rb;
struct bezier_point_vec;

/*! Represents a point on a bezier curve. These are generated with the function
 * bezier_calc_equidistant_points() and are used for rendering the snake. */
struct bezier_point
{
    struct qwpos pos; /* Position in world space */
    struct qwpos dir; /* Direction vector (normalized). Used for rotating the
                         sprite correctly */
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
struct bezier_knot
{
    struct qwpos pos;
    qa           angle;
    uint8_t      len_backwards, len_forwards;
};

void bezier_knot_init(
    struct bezier_knot* bh,
    struct qwpos        pos,
    qa                  angle,
    uint8_t             len_backwards,
    uint8_t             len_forwards);

void bezier_calc_aabb(
    struct qwaabb*            bb,
    const struct bezier_knot* head,
    const struct bezier_knot* tail);

/*!
 * \brief Performs a constrained least squares fit on the input data points to
 * generate a 3rd degree bezier curve that fits the data.
 * \param[in] head The head bezier knot will be positioned and rotated to
 * align with the data. head->len_backwards will also be updated. \param[in]
 * tail The tail bezier knot will only have its tail->len_forwards updated.
 * The angle and position are assumed to be correct from the previous bezier
 * segment. \param[in] trail A list of qwpos2 points to fit the data to. \return
 * Returns the least squared error of the fit.
 */
double bezier_fit_trail(
    struct bezier_knot*     head,
    struct bezier_knot*     tail,
    const struct qwpos_vec* trail);

/*!
 * \brief Adjusts all bezier knots in a way to cause the snake to "squeeze"
 * over time, i.e. tight circles become tighter over time.
 * \param[in,out] knots A list of all bezier knots forming the curve.
 * \param[in] sim_tick_rate Simulation tick rate.
 */
void bezier_squeeze_step(struct bezier_knot_rb* knots, int sim_tick_rate);

void bezier_squeeze_n_recent_step(
    struct bezier_knot_rb* knots, int n, int sim_tick_rate);

/*!
 * \brief Samples the curve at constant intervals and stores each position
 * into the bezier_points structure.
 * \param[out] bezier_points The resulting points are written to this array.
 * The array is cleared every time, so no need to do that before calling.
 * \param[in] knots The list of bezier knots comprising the curve.
 * \param[in] spacing The distance between each sampled point on the curve,
 * in world space.
 * \param[in] snake_length The required total length of the snake, in world
 * space.
 */
int bezier_calc_equidistant_points(
    struct bezier_point_vec**    bezier_points,
    const struct bezier_knot_rb* knots,
    qw                           spacing,
    qw                           snake_length);

int bezier_test_radius(
    const struct bezier_knot* head,
    const struct bezier_knot* tail,
    struct qwpos              pos,
    qw                        radius);

static int
bezier_knots_equal(const struct bezier_knot* a, const struct bezier_knot* b)
{
    return a->pos.x == b->pos.x && a->pos.y == b->pos.y &&
           a->angle == b->angle && a->len_backwards == b->len_backwards &&
           a->len_forwards == b->len_forwards;
}

static int
bezier_knots_pos_equal(const struct bezier_knot* a, const struct bezier_knot* b)
{
    return a->pos.x == b->pos.x && a->pos.y == b->pos.y;
}
