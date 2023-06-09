#pragma once

#include "clither/config.h"
#include "clither/q.h"
#include <stdint.h>

C_BEGIN

struct cs_vector;

struct bezier_point
{
    struct qwpos pos;
    struct qwpos dir;
};

/*!
 * \brief Represents a knot.
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
    uint8_t len_backwards, len_forwards;
};

void
bezier_handle_init(struct bezier_handle* bh, struct qwpos pos);

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
    struct cs_vector* bezier_handles,
    int sim_tick_rate);

void
bezier_squeeze_n_recent_step(
    struct cs_vector* bezier_handles,
    int n,
    int sim_tick_rate);

int
bezier_calc_equidistant_points(
    struct cs_vector* bezier_points,
    const struct cs_vector* bezier_handles,
    qw spacing,
    int snake_length);

C_END
