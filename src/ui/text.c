#include "clither/ui/ui.h"
#include "clither/util/str.h"

/* ------------------------------------------------------------------------- */
void ui_text_set_message(struct ui_element* elem, const char* message)
{
    str_set_cstr(&elem->u.text.str, message);
}

/* ------------------------------------------------------------------------- */
struct ui_text_style ui_text_style(uint32_t color, float size)
{
    struct ui_text_style style;
    style.color = color;
    style.size = size;
    return style;
}

/* ------------------------------------------------------------------------- */
struct ui_element ui_text(
    struct strview       str,
    struct fpos          pos,
    struct ui_text_style style,
    enum ui_align        align,
    void (*set_message)(struct ui_element* elem, const char* message))
{
    struct ui_element elem;
    ui_element_init(&elem, UI_TEXT);
    str_init(&elem.u.text.str);
    str_set(&elem.u.text.str, str);
    elem.u.text.pos = pos;
    elem.u.text.color = style.color;
    elem.u.text.size = style.size;
    elem.u.text.align = align;
    elem.set_message = set_message;
    return elem;
}
