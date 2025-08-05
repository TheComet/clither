#include "clither/game/input.h"
#include "clither/ui/ui.h"

/* ------------------------------------------------------------------------- */
static uint32_t
crossfade_color(uint32_t c1, uint32_t c2, int cross, int cross_max)
{
    uint8_t r = (((c1 >> 16) & 0xFF) * (cross_max - cross) +
                 ((c2 >> 16) & 0xFF) * cross) /
                cross_max;
    uint8_t g = (((c1 >> 8) & 0xFF) * (cross_max - cross) +
                 ((c2 >> 8) & 0xFF) * cross) /
                cross_max;
    uint8_t b = (((c1 >> 0) & 0xFF) * (cross_max - cross) +
                 ((c2 >> 0) & 0xFF) * cross) /
                cross_max;
    uint8_t a = (((c1 >> 24) & 0xFF) * (cross_max - cross) +
                 ((c2 >> 24) & 0xFF) * cross) /
                cross_max;
    return (a << 24) | (r << 16) | (g << 8) | (b << 0);
}

/* ------------------------------------------------------------------------- */
static int
slider_is_mouse_over(struct ui_element* elem, const struct input* input)
{
    float x1 = elem->u.slider.start.x;
    float y1 = elem->u.slider.start.y;
    float x2 = elem->u.slider.end.x;
    float y2 = elem->u.slider.end.y;
    float t = elem->u.slider.value;

    float knob_radius = elem->u.slider.knob_diameter / 2.0f;
    float knob_x = x1 + (x2 - x1) * t;
    float knob_y = y1 + (y2 - y1) * t;
    float dx = input->mousex_ar - knob_x;
    float dy = input->mousey_ar - knob_y;
    float dist_sq = dx * dx + dy * dy;
    float radius_sq = knob_radius * knob_radius;

    return dist_sq <= radius_sq;
}

/* ------------------------------------------------------------------------- */
static void slider_step_anim(
    struct ui_element* elem, const struct input* input, uint8_t sim_tick_rate)
{
    const int crossfade_period = sim_tick_rate / 12;

    elem->u.slider.knob_hover = elem->is_mouse_over(elem, input) != 0;

    if (elem->u.slider.knob_hover)
    {
        if (elem->u.slider.hover_crossfade < crossfade_period)
            elem->u.slider.hover_crossfade++;
    }
    else
    {
        if (elem->u.slider.hover_crossfade > 0)
            elem->u.slider.hover_crossfade--;
    }

    elem->u.slider.color = crossfade_color(
        elem->u.slider.normal_color,
        elem->u.slider.hover_color,
        elem->u.slider.hover_crossfade,
        crossfade_period);
}

/* ------------------------------------------------------------------------- */
struct ui_element ui_slider(
    struct fpos            start,
    struct fpos            end,
    struct ui_slider_style style,
    enum ui_cmd_type (*interact)(
        struct ui*, union ui_cmd*, struct ui_element*, struct input*))
{
    struct ui_element elem;

    ui_element_init(&elem, UI_SLIDER);
    elem.u.slider.start = start;
    elem.u.slider.end = end;
    elem.u.slider.value = 0.0;
    elem.u.slider.knob_diameter = style.knob_diameter;
    elem.u.slider.normal_color = style.normal_color;
    elem.u.slider.hover_color = style.hover_color;
    elem.u.slider.hover_crossfade = 0;
    elem.u.slider.knob_hover = 0;
    elem.u.slider.grabbed = 0;

    elem.is_mouse_over = slider_is_mouse_over;
    elem.step_anim = slider_step_anim;
    elem.interact = interact;

    return elem;
}
