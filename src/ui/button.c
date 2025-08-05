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
static void button_step_anim(
    struct ui_element* elem, const struct input* input, uint8_t sim_tick_rate)
{
    const int crossfade_period = sim_tick_rate / 12;

    if (!elem->u.button.enabled)
    {
        elem->u.button.text.color = elem->u.button.disabled_color;
        return;
    }

    if (elem->u.button.mouse_controlled && elem->is_mouse_over)
        elem->u.button.hover = elem->is_mouse_over(elem, input) != 0;

    if (elem->u.button.hover)
    {
        if (elem->u.button.hover_crossfade < crossfade_period)
            elem->u.button.hover_crossfade++;
    }
    else
    {
        if (elem->u.button.hover_crossfade > 0)
            elem->u.button.hover_crossfade--;
    }

    elem->u.button.text.color = crossfade_color(
        elem->u.button.normal_color,
        elem->u.button.hover_color,
        elem->u.button.hover_crossfade,
        crossfade_period);
}

/* ------------------------------------------------------------------------- */
struct ui_button_style make_ui_button_style(
    uint32_t color,
    uint32_t mouseover_color,
    uint32_t disabled_color,
    float    size)
{
    struct ui_button_style style;
    style.color = color;
    style.hover_color = mouseover_color;
    style.disabled_color = disabled_color;
    style.text_size = size;
    return style;
}

/* ------------------------------------------------------------------------- */
struct ui_element ui_button(
    struct strview         str,
    struct fpos            pos,
    struct ui_button_style style,
    int (*is_mouse_over)(struct ui_element*, const struct input*),
    enum ui_cmd_type (*interact)(
        struct ui*, union ui_cmd*, struct ui_element*, struct input*))
{
    struct ui_element    elem;
    struct ui_text_style text_style =
        ui_text_style(style.color, style.text_size);

    ui_element_init(&elem, UI_BUTTON);

    elem.u.button.text =
        ui_text(str, pos, text_style, UI_ALIGN_CENTER, NULL).u.text;
    elem.u.button.hover_color = style.hover_color;
    elem.u.button.disabled_color = style.disabled_color;
    elem.u.button.normal_color = style.color;
    elem.u.button.hover_crossfade = 0;
    elem.u.button.enabled = 1;
    elem.u.button.mouse_controlled = 1;
    elem.u.button.hover = 0;

    elem.is_mouse_over = is_mouse_over;
    elem.step_anim = button_step_anim;
    elem.interact = interact;

    return elem;
}
