#include "clither/bezier.h"
#include "clither/q.h"
#include "clither/snake.h"

/* ------------------------------------------------------------------------- */
void
snake_init(struct snake* s)
{
    vector_init(&s->points, sizeof(struct qpos2));
    vector_init(&s->bezier_handles, sizeof(struct bezier_handle));
    s->length = 20;
}

/* ------------------------------------------------------------------------- */
void
snake_deinit(struct snake* s)
{
    vector_deinit(&s->bezier_handles);
    vector_deinit(&s->points);
}

/* ------------------------------------------------------------------------- */
void
snake_step_forwards(
    struct snake* s,
    struct controls* c)
{

}
