#include "clither/ui/ui.h"

/* ------------------------------------------------------------------------- */
struct ui_element
ui_rectangle(struct fpos pos, struct fpos size, uint32_t color)
{
    struct ui_element elem;
    ui_element_init(&elem, UI_RECTANGLE);
    elem.u.rectangle.pos = pos;
    elem.u.rectangle.size = size;
    elem.u.rectangle.color = color;
    return elem;
}

