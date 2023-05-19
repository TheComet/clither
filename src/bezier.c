#include "clither/bezier.h"
#include "clither/log.h"

#include "cstructures/memory.h"
#include "cstructures/vector.h"

/* ------------------------------------------------------------------------- */
void
bezier_handle_init(struct bezier_handle* bh, struct qwpos2 pos, uint8_t angle, uint8_t len)
{
    bh->pos = pos;
    bh->angle = angle;
    bh->len = len;
}

/* ------------------------------------------------------------------------- */
qw
bezier_fit_head(
        struct bezier_handle* tail,
        struct bezier_handle* head,
        const struct cs_vector* points)
{
#define BUF_SIZE 128
    int i, m, n;
    qw tbuf[BUF_SIZE];
    qw rbuf[BUF_SIZE];
    qw* t;
    qw* r;
    qw T[4][4];

    /*
     * We use constrained polynomial regression to fit the bezier curve to the
     * data points.
     * https://stats.stackexchange.com/questions/50447/perform-linear-regression-but-force-solution-to-go-through-some-particular-data
     * 
     * The fitted curve must pass through the first and last points. The problem
     * can be formulated as:
     * 
     *   x = fx(t) + (t-t0)(t-t1)(cx0 + cx1*t + cx2*t^2 + cx3*t^3)
     *   y = fy(t) + (t-t0)(t-t1)(cy0 + cy1*t + cy2*t^2 + cy3*t^3)
     * 
     * where cx0..cx3 are the unknown coefficients of the x dimension of the
     * bezier curve, cy0..cy3 are the unknown coefficients of the y domension
     * of the bezier curve, (t0,x0,y0) and (t1,x1,y1) are the first and last
     * points, and fx(t) and fy(t) are polynomials (in this case 1st degree)
     * that pass through these two points. If we define:
     * 
     *   r(t) = (t-t0)(t-t1)
     * 
     * then the problem can be rewritten as:
     * 
     *   x - fx(t) = cx0*r(t) + cx1*r(t)*t + cx2*r(t)*t^2 + cx3*r(t)*t^3
     *   y - fy(t) = cy0*r(t) + cy1*r(t)*t + cy2*r(t)*t^2 + cy3*r(t)*t^3
     * 
     * The coefficients cx0..cx3 and cy0..cy3 can be independently estimated
     * using ordinary least squares estimation.
     */
    t = vector_count(points) > BUF_SIZE ? MALLOC(sizeof(qw) * vector_count(points)) : tbuf;
    r = vector_count(points) > BUF_SIZE ? MALLOC(sizeof(qw) * vector_count(points)) : rbuf;
    if (t > BUF_SIZE)
        log_warn("had to allocate memory for bezier_fit_head()\n");

    /* r(t) = (t-t0)(t-t1) */
    for (i = 0; i != vector_count(points); ++i)
    {
        qw t = make_qw(i, vector_count(points) - 1);  /* [0..1] */
        qw t0 = make_qw(0, 1);  /* t0 = 0 (pass through first point) */
        qw t1 = make_qw(1, 1);  /* t1 = 1 (pass through last point) */
        r[i] = qw_mul(qw_sub(t, t0), qw_sub(t, t1));
    }

    /*
     * / r0       r1      ... rn        / r0  r0*t0  r0*t0^2  r0*t0^3 \
     * | r0*t0    r1*t1   ... rn*tn   | | r1  r1*t1  r1*t1^2  r1*t1^3 |
     * | r0*t0^2  r1*t1^2 ... rn*tn^2 | | ..  .....  .......  ....... |
     * \ r0*t0^3  r1*t1^3 ... rn*tn^3 / \ rn  rn*tn  rn*tn^2  rn*tn^3 /
     */

    for (m = 0; m != vector_count(points); ++m)
    {
        T[i][0] = 
    }

    if (vector_count(points) > BUF_SIZE)
    {
        FREE(t);
        FREE(r);
    }

    return 0;
}
