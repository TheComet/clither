#include "clither/bezier.h"
#include "clither/log.h"

#include "cstructures/rb.h"
#include "cstructures/vector.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------------- */
static q16_16
distance_to_segment_sq(
    q16_16 px, q16_16 py, q16_16 t,
    const q16_16 Ax[4],
    const q16_16 Ay[4])
{
    q16_16 t2 = q16_16_mul(t, t);
    q16_16 t3 = q16_16_mul(t, t2);
    q16_16 x = q16_16_add(q16_16_add(q16_16_add(Ax[0], q16_16_mul(Ax[1], t)), q16_16_mul(Ax[2], t2)), q16_16_mul(Ax[3], t3));
    q16_16 y = q16_16_add(q16_16_add(q16_16_add(Ay[0], q16_16_mul(Ay[1], t)), q16_16_mul(Ay[2], t2)), q16_16_mul(Ay[3], t3));
    q16_16 dx = q16_16_sub(px, x);
    q16_16 dy = q16_16_sub(py, y);
    return dx*dx + dy*dy;  /* HACK: Not the same as q16_16_mul(), but the precision of Q16.16 isn't enough so we do this */
}
static q16_16
binary_search_min_dist_sq(
    const struct qwpos* p,
    const q16_16 Ax[4],
    const q16_16 Ay[4],
    q16_16 t_initial_guess)
{
    q16_16 last_error;
    q16_16 t = t_initial_guess;
    q16_16 t_step = make_q16_16_2(1, 2);
    q16_16 px = qw_to_q16_16(p->x);
    q16_16 py = qw_to_q16_16(p->y);

    last_error = distance_to_segment_sq(px, py, t, Ax, Ay);
    do
    {
        q16_16 error_up, error_down;
        q16_16 t_up = q16_16_add(t, t_step);
        q16_16 t_down = q16_16_sub(t, t_step);

        if (t_up <= make_q16_16(1))
            error_up = distance_to_segment_sq(px, py, t_up, Ax, Ay);
        else
            error_up = last_error;

        if (t_down >= 0)
            error_down = distance_to_segment_sq(px, py, t_down, Ax, Ay);
        else
            error_down = last_error;

        if (error_up < last_error)
        {
            last_error = error_up;
            t = t_up;
        }
        else if (error_down < last_error)
        {
            last_error = error_down;
            t = t_down;
        }

        t_step /= 2;
    } while (t_step > 0);

    return last_error;
}

/* ------------------------------------------------------------------------- */
static void
calc_coeff(
    qw* Ax, qw* Ay,
    const struct bezier_handle* head,
    const struct bezier_handle* tail,
    const struct qwpos off)
{
    /* Calculate bezier control points */
    const struct qwpos p0 = make_qwposqw(
        qw_sub(head->pos.x, off.x),
        qw_sub(head->pos.y, off.y)
    );
    const struct qwpos p3 = make_qwposqw(
        qw_sub(tail->pos.x, off.x),
        qw_sub(tail->pos.y, off.y)
    );
    const struct qwpos p1 = make_qwposqw(
        qw_add(p0.x, qw_rescale(qa_cos(head->angle), head->len_backwards, 255)),
        qw_add(p0.y, qw_rescale(qa_sin(head->angle), head->len_backwards, 255))
    );
    const struct qwpos p2 = make_qwposqw(
        qw_sub(p3.x, qw_rescale(qa_cos(tail->angle), tail->len_forwards, 255)),
        qw_sub(p3.y, qw_rescale(qa_sin(tail->angle), tail->len_forwards, 255))
    );

    const qw _3x0 = 3 * p0.x;
    const qw _3x1 = 3 * p1.x;
    const qw _6x1 = 6 * p1.x;
    const qw _3x2 = 3 * p2.x;

    const qw _3y0 = 3 * p0.y;
    const qw _3y1 = 3 * p1.y;
    const qw _6y1 = 6 * p1.y;
    const qw _3y2 = 3 * p2.y;

    /* Calculate polynomial coefficients X dimension */
    Ax[0] = p0.x;
    Ax[1] = qw_sub(_3x1, _3x0);
    Ax[2] = qw_sub(qw_add(_3x0, _3x2), _6x1);
    Ax[3] = qw_sub(qw_sub(qw_add(_3x1, p3.x), _3x2), p0.x);

    /* Calculate polynomial coefficients Y dimension */
    Ay[0] = p0.y;
    Ay[1] = qw_sub(_3y1, _3y0);
    Ay[2] = qw_sub(qw_add(_3y0, _3y2), _6y1);
    Ay[3] = qw_sub(qw_sub(qw_add(_3y1, p3.y), _3y2), p0.y);
}

/* ------------------------------------------------------------------------- */
static struct qwpos
bezier_xy(const qw Ax[4], const qw Ay[4], const qw t)
{
    /* Calculate x,y position on curve */
    const qw t2 = qw_mul(t, t);
    const qw t3 = qw_mul(t, t2);
    return make_qwposqw(
        qw_add(qw_add(qw_add(Ax[0], qw_mul(Ax[1], t)), qw_mul(Ax[2], t2)), qw_mul(Ax[3], t3)),
        qw_add(qw_add(qw_add(Ay[0], qw_mul(Ay[1], t)), qw_mul(Ay[2], t2)), qw_mul(Ay[3], t3))
    );
}

/* ------------------------------------------------------------------------- */
void
bezier_handle_init(struct bezier_handle* bh, struct qwpos pos, qa angle)
{
    bh->pos = pos;
    bh->angle = angle;
    bh->len_forwards = 0;
    bh->len_backwards = 0;
}

/* ------------------------------------------------------------------------- */
static void
bezier_calc_aabb_coeff(
    struct qwaabb* bb,
    const qw Ax[4], const qw Ay[4])
{
    qw x1 = Ax[0];
    qw x2 = qw_add(Ax[0], qw_add(Ax[1], qw_add(Ax[2], Ax[3])));

    qw y1 = Ay[0];
    qw y2 = qw_add(Ay[0], qw_add(Ay[1], qw_add(Ay[2], Ay[3])));

    qw ax = 3*Ax[3];
    qw bx = 2*Ax[2];
    qw Dx = qw_sub(qw_sq(bx), 4 * qw_mul(ax, Ax[1]));

    qw ay = 3*Ay[3];
    qw by = 2*Ay[2];
    qw Dy = qw_sub(qw_sq(by), 4 * qw_mul(ay, Ay[1]));

    if (x1 < x2)
    {
        bb->x1 = x1;
        bb->x2 = x2;
    }
    else
    {
        bb->x1 = x2;
        bb->x2 = x1;
    }

    if (y1 < y2)
    {
        bb->y1 = y1;
        bb->y2 = y2;
    }
    else
    {
        bb->y1 = y2;
        bb->y2 = y1;
    }

    if (Dx >= make_qw(0) && ax != make_qw(0))
    {
        qw D_sq = qw_sqrt(Dx);
        qw t1 = qw_div(qw_add(-bx, D_sq), 2*ax);
        qw t2 = qw_div(qw_sub(-bx, D_sq), 2*ax);
        if (t1 >= make_qw(0) && t1 <= make_qw(1))
        {
            qw t1_2 = qw_mul(t1, t1);
            qw t1_3 = qw_mul(t1_2, t1);
            qw x = qw_add(qw_add(qw_add(Ax[0], qw_mul(Ax[1], t1)), qw_mul(Ax[2], t1_2)), qw_mul(Ax[3], t1_3));
            if (bb->x1 > x) bb->x1 = x;
            if (bb->x2 < x) bb->x2 = x;
        }

        if (t2 >= make_qw(0) && t2 <= make_qw(1))
        {
            qw t2_2 = qw_mul(t2, t2);
            qw t2_3 = qw_mul(t2_2, t2);
            qw x = qw_add(qw_add(qw_add(Ax[0], qw_mul(Ax[1], t2)), qw_mul(Ax[2], t2_2)), qw_mul(Ax[3], t2_3));
            if (bb->x1 > x) bb->x1 = x;
            if (bb->x2 < x) bb->x2 = x;
        }
    }

    if (Dy >= make_qw(0) && ay != make_qw(0))
    {
        qw D_sq = qw_sqrt(Dy);
        qw t1 = qw_div(qw_add(-by, D_sq), 2*ay);
        qw t2 = qw_div(qw_sub(-by, D_sq), 2*ay);
        if (t1 >= make_qw(0) && t1 <= make_qw(1))
        {
            qw t1_2 = qw_mul(t1, t1);
            qw t1_3 = qw_mul(t1_2, t1);
            qw y = qw_add(qw_add(qw_add(Ay[0], qw_mul(Ay[1], t1)), qw_mul(Ay[2], t1_2)), qw_mul(Ay[3], t1_3));
            if (bb->y1 > y) bb->y1 = y;
            if (bb->y2 < y) bb->y2 = y;
        }

        if (t2 >= make_qw(0) && t2 <= make_qw(1))
        {
            qw t2_2 = qw_mul(t2, t2);
            qw t2_3 = qw_mul(t2_2, t2);
            qw y = qw_add(qw_add(qw_add(Ay[0], qw_mul(Ay[1], t2)), qw_mul(Ay[2], t2_2)), qw_mul(Ay[3], t2_3));
            if (bb->y1 > y) bb->y1 = y;
            if (bb->y2 < y) bb->y2 = y;
        }
    }
}
void
bezier_calc_aabb(
    struct qwaabb* bb,
    const struct bezier_handle* head,
    const struct bezier_handle* tail)
{
    qw Ax[4], Ay[4];
    calc_coeff(Ax, Ay, head, tail, head->pos);
    bezier_calc_aabb_coeff(bb, Ax, Ay);
    bb->x1 = qw_add(bb->x1, head->pos.x);
    bb->x2 = qw_add(bb->x2, head->pos.x);
    bb->y1 = qw_add(bb->y1, head->pos.y);
    bb->y2 = qw_add(bb->y2, head->pos.y);
}

/* ------------------------------------------------------------------------- */
double
bezier_fit_trail(
        struct bezier_handle* head,
        struct bezier_handle* tail,
        const struct cs_vector* trail)
{
    int i, m;
    q16_16 T[2][2];
    q16_16 T_inv[2][2];
    q16_16 Ax[4], Ay[4];
    q16_16 Cx[2], Cy[2];
    uint64_t mse_error;
    q16_16 det;
    q16_16 mx, qx, my, qy;  /* f(t) coefficients */

    struct qwpos* p0 = vector_front(trail);  /* tail */
    struct qwpos* pm = vector_back(trail);   /* head */

    /*
     * Cubic bezier curve fitting requires at least 5 points for polynomial
     * regression. The following special cases calculate the coefficients
     * directly when there are 4 or less points.
     */
    if (vector_count(trail) <= 2)
    {
        head->pos = *pm;
        head->angle = tail->angle;
        head->len_backwards = 0;
        tail->len_forwards = 0;
        return 0;
    }
    if (vector_count(trail) == 3)
    {
        struct qwpos* p1 = vector_get(trail, 1);
        qw head_dx = qw_sub(p1->x, pm->x);
        qw head_dy = qw_sub(p1->y, pm->y);
        qw tail_dx = qw_sub(p0->x, p1->x);
        qw tail_dy = qw_sub(p0->y, p1->y);
        qw head_lensq = qw_add(qw_mul(head_dx, head_dx), qw_mul(head_dy, head_dy));
        qw tail_lensq = qw_add(qw_mul(tail_dx, tail_dx), qw_mul(tail_dy, tail_dy));
        double head_len = sqrt(qw_to_float(head_lensq));
        double tail_len = sqrt(qw_to_float(tail_lensq));

        head->pos = *pm;
        head->angle = make_qa(atan2(qw_to_float(head_dy), qw_to_float(head_dx)));
        head->len_backwards = (uint8_t)(head_len * 255.0);

        tail->len_forwards = (uint8_t)(tail_len * 255.0);

        return 0;
    }
    if (vector_count(trail) == 4)
    {
        struct qwpos* p1 = vector_get(trail, 1);
        struct qwpos* p2 = vector_get(trail, 2);
        qw head_dx = qw_sub(p2->x, pm->x);
        qw head_dy = qw_sub(p2->y, pm->y);
        qw tail_dx = qw_sub(p0->x, p1->x);
        qw tail_dy = qw_sub(p0->y, p1->y);
        qw head_lensq = qw_add(qw_mul(head_dx, head_dx), qw_mul(head_dy, head_dy));
        qw tail_lensq = qw_add(qw_mul(tail_dx, tail_dx), qw_mul(tail_dy, tail_dy));
        double head_len = sqrt(qw_to_float(head_lensq));
        double tail_len = sqrt(qw_to_float(tail_lensq));

        head->pos = *pm;
        head->angle = make_qa(atan2(qw_to_float(head_dy), qw_to_float(head_dx)));
        head->len_backwards = (uint8_t)(head_len * 255.0);

        tail->len_forwards = (uint8_t)(tail_len * 255.0);

        return 0;
    }

    /*
     * We use constrained polynomial regression to fit the bezier curve to the
     * data points.
     * https://stats.stackexchange.com/questions/50447/perform-linear-regression-but-force-solution-to-go-through-some-particular-data
     *
     * The fitted curve must pass through the first and last points. The problem
     * can be formulated as:
     *
     *   x = fx(t) + (t-t0)(t-tm)(cx0 + cx1*t)
     *   y = fy(t) + (t-t0)(t-tm)(cy0 + cy1*t)
     *
     * where cx0..cx1 are the unknown coefficients of the x dimension of the
     * bezier curve, cy0..cy1 are the unknown coefficients of the y dimension
     * of the bezier curve, (t0,x0,y0) and (tm,xm,ym) are the first and last
     * points, and fx(t) and fy(t) are polynomials (in this case 1st degree)
     * that pass through these two points. If we define:
     *
     *   r(t) = (t-t0)(t-tm)
     *
     * then the problem can be rewritten as:
     *
     *   (x - fx(t)) / r(t) = cx0 + cx1*t
     *   (y - fy(t)) / r(t) = cy0 + cy1*t
     *
     * Notice, however, that r(t) has singularities at t0 and tm, and therefore,
     * we must build the T matrix (see below) excluding those datapoints.
     *
     * The coefficients cx0..cx1 and cy0..cy1 can be independently estimated
     * using ordinary least squares estimation. For a polynomial of the form
     *
     *   x(t) = c0 + c1*t + c2*t^2 + c3*t^3
     *
     * rewritten in matrix form X = T*C + E
     *
     *   [ x0 ]   [ 1  t0 ]          [ e0 ]
     *   [ x1 ] = [ 1  t1 ] [ c0 ] + [ e1 ]
     *   [ .. ]   [ .  .. ] [ c1 ]   [ e2 ]
     *   [ xm ]   [ 1  tm ]          [ e3 ]
     *
     * Least squares estimation of c0 and c1 can be computed with:
     *
     *   C = (T'*T)^-1 * T' * X
     */

    /*
     * Calculate T'*T:
     *
     *                   [ 1   t0 ]
     * [ 1   1  ... 1  ] [ 1   t1 ]
     * [ t0  t1 ... tn ] [ ..  .. ]
     *                   [ 1   tn ]
     */
    memset(T, 0, sizeof(T));
    for (i = 1; i < (int)vector_count(trail) - 1; ++i)
    {
        /* t = [0..1] */
        q16_16 t = make_q16_16_2(i, vector_count(trail) - 1);
        q16_16 t2 = q16_16_mul(t, t);

        T[0][0] = q16_16_add(T[0][0], make_q16_16(1));
        T[0][1] = q16_16_add(T[0][1], t);
        T[1][0] = q16_16_add(T[1][0], t);
        T[1][1] = q16_16_add(T[1][1], t2);
    }

    /*
     * Calculate inverse (T'*T)^-1
     *
     *          -1       1
     *   [ a b ]    = ------- [  d -b ]
     *   [ c d ]      ad - bc [ -c  a ]
     */
    det = q16_16_sub(q16_16_mul(T[0][0], T[1][1]), q16_16_mul(T[0][1], T[1][0]));
    if (det == 0)
    {
        /*
         * Usually means there are too many data points and we've exceeded the
         * precision of q16.16. Kind of ugly but the best we can do is update
         * the head position (hoping it still fits the data) and return an
         * error estimate that will definitely cause a new segment to be created
         */
        head->pos = *pm;
        return (1<<30);
    }
    T_inv[0][0] = q16_16_div(T[1][1], det);
    T_inv[0][1] = q16_16_div(-T[0][1], det);
    T_inv[1][0] = q16_16_div(-T[1][0], det);
    T_inv[1][1] = q16_16_div(T[0][0], det);

    /*
     * Calculate f(t) coefficients
     *   f(t) = mx*x + qx
     *
     *   mx = (xm-x0) / (tm-t0)
     *   qx = x0 - mx*t0
     */
    /* NOTE: t0=0, tm=1 -> fm = xm-x0, fq = x0 */
    mx = qw_to_q16_16(qw_sub(pm->x, p0->x));
    qx = qw_to_q16_16(p0->x);
    my = qw_to_q16_16(qw_sub(pm->y, p0->y));
    qy = qw_to_q16_16(p0->y);

    /*
     * (T'*T)^-1 * T' * X = T_inv * T' * X
     *
     *   T' = [ 1   1  ... 1  ]
     *        [ t0  t1 ... tn ]
     */
    memset(Cx, 0, sizeof(Cx));
    memset(Cy, 0, sizeof(Cy));
    for (i = 1; i < (int)vector_count(trail) - 1; ++i)
    {
        /* t = [0..1] */
        q16_16 t = make_q16_16_2(i, vector_count(trail) - 1);

        /* r(t) = (t-t0)(t-tm) = (t-0)(t-1) = t(t-1) */
        q16_16 tm = make_q16_16(1);  /* tm = 1 (pass through last point) */
        q16_16 r = q16_16_mul(t, q16_16_sub(t, tm));

        /* f = m*t + q */
        q16_16 fx = q16_16_add(q16_16_mul(mx, t), qx);
        q16_16 fy = q16_16_add(q16_16_mul(my, t), qy);

        /* X = (x - f) / r */
        struct qwpos* p = vector_get(trail, i);
        q16_16 x = q16_16_div(q16_16_sub(qw_to_q16_16(p->x), fx), r);
        q16_16 y = q16_16_div(q16_16_sub(qw_to_q16_16(p->y), fy), r);
        for (m = 0; m != 2; ++m)
        {
            q16_16 c = q16_16_add(T_inv[m][0], q16_16_mul(T_inv[m][1], t));
            q16_16 cx = q16_16_mul(c, x);
            q16_16 cy = q16_16_mul(c, y);

            Cx[m] = q16_16_add(Cx[m], cx);
            Cy[m] = q16_16_add(Cy[m], cy);
        }
    }

    /*
     * Convert fitted coefficients Cx and Cy to bezier handle coordinates
     * x0, x1, x2, x3 and y0, y1, y2, y3.
     *
     * The fitted polynomial is:
     *
     *   x = fx(t) + r(t)(cx0 + cx1*t)
     *   x = mx*t + qx + (t-0)(t-1)(cx0 + cx1*t)
     *
     * Expanded and re-arranged:
     *
     *   x = qx + (mx-cx0)*t + (cx0-cx1)*t^2 + cx1*t^3
     *
     * By comparing this polynomial to the bezier polynomial with control points
     * x0..x3:
     *
     *   x = x0 + (-3*x0 + 3*x1)t + (3*x0 - 6*x1 + 3*x2)t^2 + (-x0 + 3*x1 - 3*x2 + x3)t^3
     *
     * we can create the following system of equations to relate the set of
     * coefficients:
     *
     *   qx        = x0                           x0 = qx
     *   mx - cx0  = -3*x0 + 3*x1            -->  x1 = (mx - cx0 + 3*x0) / 3
     *   cx0 - cx1 = 3*x0 - 6*x1 + 3*x2           x2 = (cx0 - cx1 - 3*x0 + 6*x1) / 3
     *   cx1       = -x0 + 3*x1 - 3*x2 + x3       x3 = cx1 + x0 - 3*x1 + 3*x2
     */
    {
        /* X dimension control points */
        q16_16 x0 = qx;
        q16_16 _3x0 = 3 * x0;
        q16_16 x1 = q16_16_add(q16_16_sub(mx, Cx[0]), _3x0) / 3;
        q16_16 _6x1 = 6 * x1;
        q16_16 x2 = q16_16_add(q16_16_sub(q16_16_sub(Cx[0], Cx[1]), _3x0), _6x1) / 3;
        q16_16 _3x1 = 3 * x1;
        q16_16 _3x2 = 3 * x2;
        q16_16 x3 = q16_16_add(q16_16_sub(q16_16_add(Cx[1], x0), _3x1), _3x2);

        /* Y dimension control points */
        q16_16 y0 = qy;
        q16_16 _3y0 = 3 * y0;
        q16_16 y1 = q16_16_add(q16_16_sub(my, Cy[0]), _3y0) / 3;
        q16_16 _6y1 = 6 * y1;
        q16_16 y2 = q16_16_add(q16_16_sub(q16_16_sub(Cy[0], Cy[1]), _3y0), _6y1) / 3;
        q16_16 _3y1 = 3 * y1;
        q16_16 _3y2 = 3 * y2;
        q16_16 y3 = q16_16_add(q16_16_sub(q16_16_add(Cy[1], y0), _3y1), _3y2);

        /* Control points are stored as polar coordinates relative to head/tail */
        q16_16 tail_dx = q16_16_sub(x1, x0);
        q16_16 tail_dy = q16_16_sub(y1, y0);
        q16_16 head_dx = q16_16_sub(x2, x3);
        q16_16 head_dy = q16_16_sub(y2, y3);
        q16_16 head_lensq = q16_16_add(q16_16_mul(head_dx, head_dx), q16_16_mul(head_dy, head_dy));
        q16_16 tail_lensq = q16_16_add(q16_16_mul(tail_dx, tail_dx), q16_16_mul(tail_dy, tail_dy));
        double head_len = sqrt(q16_16_to_float(head_lensq));
        double tail_len = sqrt(q16_16_to_float(tail_lensq));

        /* Update head knot */
        head->pos = *pm;
        head->angle = make_qa(atan2(q16_16_to_float(head_dy), q16_16_to_float(head_dx)));
        head->len_backwards = (uint8_t)(head_len * 255.0);

        /*
         * We are allowed to modify the tail's "forwards length", but not the
         * angle, because the angle is shared between two bezier curves. Need
         * to make sure to factor this in to the error calculation later.
         */
        tail->len_forwards = (uint8_t)(tail_len * 255.0);

        /*
         * Modifying the tail length influences x1,y1. Propagate these changes
         * back to a set of polynomial coefficients Ax0..Ax3 and Ay0..Ay3 so
         * that error estimation is accurate.
         */
        x1 = q16_16_add(x0, make_q16_16(tail->len_forwards * -cos(qa_to_float(tail->angle)) / 255));
        y1 = q16_16_add(y0, make_q16_16(tail->len_forwards * -sin(qa_to_float(tail->angle)) / 255));

        /*
         * Calculate new polynomial coefficients:
         *
         *   x = Ax[0] + Ax[1]*t + Ax[2]*t^2 + Ax[3]*t^3
         *   y = Ay[0] + Ay[1]*t + Ay[2]*t^2 + Ay[3]*t^3
         *
         * these are used for error estimation
         */
        _3x1 = 3 * x1;
        _6x1 = 6 * x1;
        Ax[0] = x0;
        Ax[1] = q16_16_sub(_3x1, _3x0);
        Ax[2] = q16_16_add(q16_16_sub(_3x0, _6x1), _3x2);
        Ax[3] = q16_16_add(q16_16_sub(q16_16_sub(_3x1, x0), _3x2), x3);

        _3y1 = 3 * y1;
        _6y1 = 6 * y1;
        Ay[0] = y0;
        Ay[1] = q16_16_sub(_3y1, _3y0);
        Ay[2] = q16_16_add(q16_16_sub(_3y0, _6y1), _3y2);
        Ay[3] = q16_16_add(q16_16_sub(q16_16_sub(_3y1, y0), _3y2), y3);
    }

    /* Error estimation */
    mse_error = 0;
    for (i = 1; i < (int)vector_count(trail) - 1; ++i)
    {
        /* t = [0..1] */
        q16_16 t = make_q16_16_2(i, vector_count(trail) - 1);

        const struct qwpos* p = vector_get(trail, i);
        mse_error += binary_search_min_dist_sq(p, Ax, Ay, t);
    }

    return q16_16_div(mse_error, make_q16_16(vector_count(trail) - 1));
}

/* ------------------------------------------------------------------------- */
void
bezier_squeeze_step(
    struct cs_rb* bezier_handles,
    int sim_tick_rate)
{

}

/* ------------------------------------------------------------------------- */
void
bezier_squeeze_n_recent_step(
    struct cs_rb* bezier_handles,
    int n,
    int sim_tick_rate)
{

}

/* ------------------------------------------------------------------------- */
int
bezier_calc_equidistant_points(
    struct cs_vector* bezier_points,
    const struct cs_rb* bezier_handles,
    qw spacing,
    qw snake_length)
{
    int i;

    const qw spacing_sq = qw_mul(spacing, spacing);
    qw total_spacing = 0;

    /*
     * Initial x,y positions
     * Calculating coefficients far away from 0,0 results in precision issues,
     * so we translate everything to 0,0 first, calculate, then translate
     * the result back.
     */
    struct qwpos pos = make_qwposi(0, 0);
    struct qwpos off = ((struct bezier_handle*)rb_peek_write(bezier_handles))->pos;

    /* Insert first point */
    vector_clear(bezier_points);
    {
        struct bezier_point* bp = vector_emplace(bezier_points);
        struct bezier_handle* head = rb_peek_write(bezier_handles);
        bp->pos.x = qw_add(pos.x, off.x);
        bp->pos.y = qw_add(pos.y, off.y);
        bp->dir.x = -qa_cos(head->angle);
        bp->dir.y = -qa_sin(head->angle);
    }

    for (i = rb_count(bezier_handles) - 2; i >= 0; --i)
    {
        const struct bezier_handle* head = rb_peek(bezier_handles, i+1);
        const struct bezier_handle* tail = rb_peek(bezier_handles, i+0);

        qw t = make_qw(0);  /* Begin search at head of curve */
        qw last_t = make_qw(0);

        qw Ax[4], Ay[4];
        calc_coeff(Ax, Ay, head, tail, off);

        while (1)
        {
            qw t_step = make_qw2(1, 2);
            while (1)
            {
                /* Calculate x,y position on curve */
                const struct qwpos next = bezier_xy(Ax, Ay, t);

                /* Check distance to previous calculated position */
                const qw dx = qw_sub(pos.x, next.x);
                const qw dy = qw_sub(pos.y, next.y);
                const qw dist_sq = qw_add(qw_mul(dx, dx), qw_mul(dy, dy));
                if (dist_sq > spacing_sq)
                    t = qw_sub(t, t_step);
                else
                    t = qw_add(t, t_step);

                if (t >= make_qw(1))
                    t = make_qw(1) - 1;  /* t=1 means we'd be on the next curve segment */
                if (t < last_t)
                    t = last_t;

                t_step /= 2;
                if (t_step == 0)
                {
                    const qw t2 = qw_mul(t, t);
                    struct bezier_point* bp;
                    if (t == make_qw(1) - 1)
                        goto next_segment;

                    /* Insert new point and calculate tangent vector */
                    bp = vector_emplace(bezier_points);
                    bp->pos.x = qw_add(next.x, off.x);
                    bp->pos.y = qw_add(next.y, off.y);
                    bp->dir.x = -qw_add(qw_add(Ax[1], qw_mul(make_qw(2), qw_mul(Ax[2], t))), qw_mul(make_qw(3), qw_mul(Ax[3], t2)));
                    bp->dir.y = -qw_add(qw_add(Ay[1], qw_mul(make_qw(2), qw_mul(Ay[2], t))), qw_mul(make_qw(3), qw_mul(Ay[3], t2)));
                    bp->dir = qwpos_normalize(bp->dir);

                    total_spacing = qw_add(total_spacing, qw_sqrt(dist_sq));
                    if (total_spacing >= snake_length)
                        return i;

                    pos = next;
                    break;
                }
            }
            last_t = t;
        }
        next_segment:;
    }

    return 0;
}
