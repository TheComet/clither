#pragma once

#include "clither/game/q.h"

struct qwpos_vec;
struct bezier_knot_rb;
struct bezier_point_vec;
struct bezier_segment_rb;

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

/* These are derived from bezier_knot angle and len_backwards/len_forwards, and
 * are not transmitted over the network. */
struct bezier_segment
{
    struct qwpos p[4];     /* p[0] = head, p[3] = tail */
    struct qwpos coeff[3]; /* Bezier coefficients to backwards knot */
    /* NOTE: Bezier coefficients are calculated relative to the head position,
     * i.e. if a*t^3 + b*t^2 + c*t + d is the bezier curve, then d will always
     * equal 0. This is why there are only 3 coefficients stored instead of 4.
     * It is recommended to perform calculations in local space and transform
     * the result back to world space by adding p[0] to the result. Calculating
     * far away from 0,0 results in fixed point overflow issues. */
    struct qwpos fallback_tangent;
};

void bezier_knot_init(
    struct bezier_knot* knot,
    struct qwpos        pos,
    qa                  angle,
    uint8_t             len_backwards,
    uint8_t             len_forwards);

void bezier_calc_segment(
    struct bezier_segment*    segment,
    const struct bezier_knot* head,
    const struct bezier_knot* tail);

qw bezier_segment_calc_length(
    const struct bezier_segment* segment, qw t_step);

void bezier_calc_aabb(struct qwaabb* bb, const struct bezier_segment* segment);

struct qwpos bezier_xy(const struct bezier_segment* segment, const qw t);
struct qwpos bezier_tangent(const struct bezier_segment* segment, const qw t);

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
 * \brief Samples the curve at constant intervals until the total length is
 * reached.
 * \param[in] knots The list of bezier knots comprising the curve.
 * \param[in] spacing The distance between each sampled point on the curve,
 * in world space.
 * \param[in] snake_length The required total length of the snake, in world
 * space.
 */
struct bezier_sample
{
    const struct bezier_segment_rb* segments;
    int                             segment_idx;

    struct qwpos pos;

    qw spacing_sq;
    qw total_spacing;
    qw snake_length;
    qw t;
};
void bezier_sample_begin(
    struct bezier_sample*           it,
    const struct bezier_segment_rb* segments,
    qw                              spacing,
    qw                              snake_length);
void bezier_sample_next(struct bezier_sample* it);
#define bezier_sample_end(it)           ((it)->t < 0)
#define bezier_sample_segment(it)       (rb_peek((it)->segments, (it)->segment_idx))
#define bezier_sample_idx(it) ((it)->segment_idx)

int bezier_test_radius(
    const struct bezier_segment* segment, struct qwpos pos, qw radius);

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
