#include "clither/controls.h"
#include "clither/log.h"
#include "clither/wrap.h"

#include <string.h>

/* ------------------------------------------------------------------------- */
void
input_init(struct input* i)
{
    memset(i, 0, sizeof *i);
}

/* ------------------------------------------------------------------------- */
void
controls_init(struct controls* c)
{
    c->angle = 128;
    c->speed = 0;
    c->action = CONTROLS_ACTION_NONE;
}

/* ------------------------------------------------------------------------- */
void
controls_rb_init(struct controls_rb* rb)
{
    rb_init(&rb->rb, sizeof(struct controls));
    controls_init(&rb->last_predicted);
    rb->first_frame_number = 0;
}

/* ------------------------------------------------------------------------- */
void
controls_rb_deinit(struct controls_rb* rb)
{
    rb_deinit(&rb->rb);
}

/* ------------------------------------------------------------------------- */
void
controls_rb_put(
    struct controls_rb* rb,
    const struct controls* controls,
    uint16_t frame_number)
{
    if (rb_count(&rb->rb) > 0)
    {
        uint16_t expected_frame = rb->first_frame_number + rb_count(&rb->rb);
        if (expected_frame != frame_number)
            return;
    }
    else
    {
        rb->first_frame_number = frame_number;
    }

    rb_put(&rb->rb, controls);
}

/* ------------------------------------------------------------------------- */
const struct controls*
controls_rb_take_or_predict(struct controls_rb* rb, uint16_t frame_number)
{
    struct controls* controls;
    struct controls* prev_controls = NULL;
    uint16_t prev_frame = frame_number - 1;
    uint16_t frame = rb->first_frame_number;
    while (rb_count(&rb->rb) > 0)
    {
        controls = rb_take(&rb->rb);
        rb->first_frame_number++;
        if (frame == prev_frame)
            prev_controls = controls;
        else if (frame == frame_number)
        {
            rb->last_predicted = *controls;
            return controls;
        }
    }

    log_dbg("controls_rb_take_or_predict(): No controls for frame %d, predicting...\n",
        frame_number);

    if (prev_controls == NULL)
        return &rb->last_predicted;

    rb->last_predicted = *prev_controls;
    return prev_controls;
}

/* ------------------------------------------------------------------------- */
const struct controls*
controls_rb_find_or_predict(const struct controls_rb* rb, uint16_t frame_number)
{
    struct controls* prev_controls = NULL;
    uint16_t prev_frame = frame_number - 1;
    uint16_t frame = rb->first_frame_number;
    RB_FOR_EACH(&rb->rb, struct controls, controls)
        if (frame == prev_frame)
            prev_controls = controls;
        else if (frame == frame_number)
            return controls;
        frame++;
    RB_END_EACH

    log_dbg("controls_rb_find_or_predict(): No controls for frame %d, predicting...\n",
        frame_number);

    if (prev_controls == NULL)
        return &rb->last_predicted;

    return prev_controls;
}
