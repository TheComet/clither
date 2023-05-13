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
snake_update_controls(struct snake* s, const struct input* i)
{
    double head_x = 400;
    double head_y = 400;
    double dx = head_x - i->mousex;
    double dy = head_y - i->mousey;
    double a = atan2(dy, dx) / (2*M_PI) + 0.5;
    double d = sqrt(dx*dx + dy*dy);
    if (d > 255)
        d = 255;
    s->controls.angle = (uint8_t)(a * 255);
    s->controls.distance = (uint8_t)d;
}

/* ------------------------------------------------------------------------- */
void
snake_step(struct snake* s, int sim_tick_rate)
{
}
