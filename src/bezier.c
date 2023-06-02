#include "clither/bezier.h"
#include "clither/log.h"

#include "cstructures/vector.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------------- */
static q16_16
squared_distance_to_polys(
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
binary_search_lsq(
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

    last_error = squared_distance_to_polys(px, py, t, Ax, Ay);
    do
    {
        q16_16 error_up, error_down;
        q16_16 t_up = q16_16_add(t, t_step);
        q16_16 t_down = q16_16_sub(t, t_step);

        if (t_up <= make_q16_16(1))
            error_up = squared_distance_to_polys(px, py, t_up, Ax, Ay);
        else
            error_up = last_error;

        if (t_down >= 0)
            error_down = squared_distance_to_polys(px, py, t_down, Ax, Ay);
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
void
bezier_handle_init(struct bezier_handle* bh, struct qwpos pos)
{
    bh->pos = pos;
    bh->angle = 0;
    bh->len_forwards = 0;
    bh->len_backwards = 0;
}

/* ------------------------------------------------------------------------- */
double
bezier_fit_head(
        struct bezier_handle* head,
        struct bezier_handle* tail,
        const struct cs_vector* points)
{
    int i, m;
    q16_16 T[2][2];
    q16_16 T_inv[2][2];
    q16_16 Ax[4], Ay[4];
    q16_16 Cx[2], Cy[2];
    uint64_t mse_error;
    q16_16 det;
    q16_16 mx, qx, my, qy;  /* f(t) coefficients */

    struct qwpos* p0 = vector_front(points);  /* tail */
    struct qwpos* pm = vector_back(points);   /* head */

    if (vector_count(points) <= 2)
    {
        head->pos = *pm;
        head->angle = qa_add(tail->angle, make_qa(M_PI));
        head->len_backwards = 0;
        tail->len_forwards = 0;
        return 0;
    }
    if (vector_count(points) == 3)
    {
        struct qwpos* p1 = vector_get_element(points, 1);
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
    if (vector_count(points) == 4)
    {
        struct qwpos* p1 = vector_get_element(points, 1);
        struct qwpos* p2 = vector_get_element(points, 2);
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
    for (i = 1; i < (int)vector_count(points) - 1; ++i)
    {
        /* t = [0..1] */
        q16_16 t = make_q16_16_2(i, vector_count(points) - 1);
        q16_16 t2 = q16_16_mul(t, t);

        T[0][0] = q16_16_add(T[0][0], make_q16_16(1));
        T[0][1] = q16_16_add(T[0][1], t);
        T[1][0] = q16_16_add(T[1][0], t);
        T[1][1] = q16_16_add(T[1][1], t2);
    }
    log_bezier("T = [%f, %f; %f, %f];\n", q16_16_to_float(T[0][0]), q16_16_to_float(T[0][1]), q16_16_to_float(T[1][0]), q16_16_to_float(T[1][1]));

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

    log_bezier("T_inv = [%f, %f; %f, %f];\n", q16_16_to_float(T_inv[0][0]), q16_16_to_float(T_inv[0][1]), q16_16_to_float(T_inv[1][0]), q16_16_to_float(T_inv[1][1]));

    /*
     * Calculate f(t) coefficients
     *   f(t) = fm*x + fq
     *
     *   fm = (xm-x0) / (tm-t0)
     *   fq = x0 - fm*t0
     */
    /* NOTE: t0=0, tm=1 -> fm = xm-x0, fq = x0 */
    mx = qw_to_q16_16(qw_sub(pm->x, p0->x));
    qx = qw_to_q16_16(p0->x);
    my = qw_to_q16_16(qw_sub(pm->y, p0->y));
    qy = qw_to_q16_16(p0->y);
    log_bezier("mx = %f; qx = %f\n", q16_16_to_float(mx), q16_16_to_float(qx));
    log_bezier("my = %f; qy = %f\n", q16_16_to_float(my), q16_16_to_float(qy));

    /*
     * (T'*T)^-1 * T' * X = T_inv * T' * X
     *
     *   T' = [ 1     1    ... 1  ]
     *        [ t0    t1   ... tn ]
     */
    memset(Cx, 0, sizeof(Cx));
    memset(Cy, 0, sizeof(Cy));
    for (i = 1; i < (int)vector_count(points) - 1; ++i)
    {
        /* t = [0..1] */
        q16_16 t = make_q16_16_2(i, vector_count(points) - 1);

        /* r(t) = (t-t0)(t-tm) = (t-0)(t-1) = t(t-1) */
        q16_16 tm = make_q16_16(1);  /* tm = 1 (pass through last point) */
        q16_16 r = q16_16_mul(t, q16_16_sub(t, tm));

        /* f = m*t + q */
        q16_16 fx = q16_16_add(q16_16_mul(mx, t), qx);
        q16_16 fy = q16_16_add(q16_16_mul(my, t), qy);

        /* X = (x - f) / r */
        struct qwpos* p = vector_get_element(points, i);
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
        q16_16 _3 = make_q16_16(3);
        q16_16 _6 = make_q16_16(6);

        /* X dimension control points */
        q16_16 x0 = qx;
        q16_16 _3x0 = q16_16_mul(_3, x0);
        q16_16 x1 = q16_16_div(q16_16_add(q16_16_sub(mx, Cx[0]), _3x0), _3);
        q16_16 _6x1 = q16_16_mul(_6, x1);
        q16_16 x2 = q16_16_div(q16_16_add(q16_16_sub(q16_16_sub(Cx[0], Cx[1]), _3x0), _6x1), _3);
        q16_16 _3x1 = q16_16_mul(_3, x1);
        q16_16 _3x2 = q16_16_mul(_3, x2);
        q16_16 x3 = q16_16_add(q16_16_sub(q16_16_add(Cx[1], x0), _3x1), _3x2);

        /* Y dimension control points */
        q16_16 y0 = qy;
        q16_16 _3y0 = q16_16_mul(y0, _3);
        q16_16 y1 = q16_16_div(q16_16_add(q16_16_sub(my, Cy[0]), _3y0), _3);
        q16_16 _6y1 = q16_16_mul(_6, y1);
        q16_16 y2 = q16_16_div(q16_16_add(q16_16_sub(q16_16_sub(Cy[0], Cy[1]), _3y0), _6y1), _3);
        q16_16 _3y1 = q16_16_mul(_3, y1);
        q16_16 _3y2 = q16_16_mul(_3, y2);
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

        log_bezier("x = [%f, %f, %f, %f];\n", q16_16_to_float(x0), q16_16_to_float(x1), q16_16_to_float(x2), q16_16_to_float(x3));
        log_bezier("y = [%f, %f, %f, %f];\n", q16_16_to_float(y0), q16_16_to_float(y1), q16_16_to_float(y2), q16_16_to_float(y3));

        /*
         * Calculate new polynomial coefficients:
         *
         *   x = Ax[0] + Ax[1]*t + Ax[2]*t^2 + Ax[3]*t^3
         *   y = Ay[0] + Ay[1]*t + Ay[2]*t^2 + Ay[3]*t^3
         *
         * these are used for error estimation
         */
        _3x1 = q16_16_mul(_3, x1);
        _6x1 = q16_16_mul(_6, x1);
        Ax[0] = x0;
        Ax[1] = q16_16_sub(_3x1, _3x0);
        Ax[2] = q16_16_add(q16_16_sub(_3x0, _6x1), _3x2);
        Ax[3] = q16_16_add(q16_16_sub(q16_16_sub(_3x1, x0), _3x2), x3);

        _3y1 = q16_16_mul(_3, y1);
        _6y1 = q16_16_mul(_6, y1);
        Ay[0] = y0;
        Ay[1] = q16_16_sub(_3y1, _3y0);
        Ay[2] = q16_16_add(q16_16_sub(_3y0, _6y1), _3y2);
        Ay[3] = q16_16_add(q16_16_sub(q16_16_sub(_3y1, y0), _3y2), y3);
    }

    log_bezier("Ax = [%f, %f, %f, %f];\n", q16_16_to_float(Ax[0]), q16_16_to_float(Ax[1]), q16_16_to_float(Ax[2]), q16_16_to_float(Ax[3]));
    log_bezier("Ay = [%f, %f, %f, %f];\n", q16_16_to_float(Ay[0]), q16_16_to_float(Ay[1]), q16_16_to_float(Ay[2]), q16_16_to_float(Ay[3]));

    log_bezier("px = [");
    for (i = 0; i < (int)vector_count(points); ++i)
    {
        struct qwpos* p = vector_get_element(points, i);
        if (i != 0)
            log_bezier(", ");
        log_bezier("%f", qw_to_float(p->x));
    }
    log_bezier("];\n");

    log_bezier("py = [");
    for (i = 0; i < (int)vector_count(points); ++i)
    {
        struct qwpos* p = vector_get_element(points, i);
        if (i != 0)
            log_bezier(", ");
        log_bezier("%f", qw_to_float(p->y));
    }
    log_bezier("];\n");

    /* Error estimation */
    mse_error = 0;
    for (i = 1; i < (int)vector_count(points) - 1; ++i)
    {
        /* t = [0..1] */
        q16_16 t = make_q16_16_2(i, vector_count(points) - 1);

        const struct qwpos* p = vector_get_element(points, i);
        mse_error += binary_search_lsq(p, Ax, Ay, t);
    }

    return q16_16_div(mse_error, make_q16_16(vector_count(points) - 1));
}

/* ------------------------------------------------------------------------- */
void
bezier_squeeze_step(
    struct cs_vector* bezier_handles,
    int sim_tick_rate)
{

}

/* ------------------------------------------------------------------------- */
void
bezier_squeeze_n_recent_step(
    struct cs_vector* bezier_handles,
    int n,
    int sim_tick_rate)
{

}

/* ------------------------------------------------------------------------- */
int
bezier_calc_equidistant_points(
    struct cs_vector* bezier_points,
    const struct cs_vector* bezier_handles,
    qw distance,
    int snake_length)
{


    return 0;
}
