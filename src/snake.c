#include "clither/bezier.h"
#include "clither/q.h"
#include "clither/snake.h"

#include "cstructures/memory.h"

#include <string.h>

/* ------------------------------------------------------------------------- */
void
snake_init(struct snake* s, const char* name)
{
    vector_init(&s->points, sizeof(struct qpos2));
    vector_init(&s->bezier_handles, sizeof(struct bezier_handle));
    s->length = 20;
    s->name = MALLOC(strlen(name) + 1);
    strcpy(s->name, name);
}

/* ------------------------------------------------------------------------- */
void
snake_deinit(struct snake* s)
{
    FREE(s->name);
    vector_deinit(&s->bezier_handles);
    vector_deinit(&s->points);
}

/* ------------------------------------------------------------------------- */
void
snake_step_forwards(
    struct snake* s,
    struct controls* c)
{
    (void)s;
    (void)c;
}
