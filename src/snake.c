#include "clither/bezier.h"
#include "clither/log.h"
#include "clither/q.h"
#include "clither/snake.h"

#include "cstructures/memory.h"

#define _USE_MATH_DEFINES
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------------- */
void
snake_init(struct snake* s, const char* name)
{
    s->name = MALLOC(strlen(name) + 1);
    strcpy(s->name, name);

    controls_init(&s->controls);

    s->head_pos = make_qpos2(0, 0);

    vector_init(&s->points, sizeof(struct qpos2));
    vector_init(&s->bezier_handles, sizeof(struct bezier_handle));

    s->length = 20;
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
snake_update_controls(struct snake* s, struct qpos2 mouse_world)
{
    double dx = q_to_float(q_sub(mouse_world.x, s->head_pos.x));
    double dy = q_to_float(q_sub(mouse_world.y, s->head_pos.y));
    double a = atan2(dy, dx) / (2*M_PI) + 0.5;
    double d = sqrt(dx*dx + dy*dy);
    if (d > 0.5)
        d = 0.5;
    s->controls.angle = (uint8_t)(a * 255);
    s->controls.distance = (uint8_t)(d * 511);

    s->head_pos.x = q_add(s->head_pos.x, 1);
}

/* ------------------------------------------------------------------------- */
void
snake_step(struct snake* s, int sim_tick_rate)
{
}
