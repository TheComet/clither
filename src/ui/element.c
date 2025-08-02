#include "clither/ui/ui.h"

/* ------------------------------------------------------------------------- */
void ui_element_init(struct ui_element* elem, enum ui_element_type type)
{
    elem->is_mouse_over = NULL;
    elem->step_anim = NULL;
    elem->interact = NULL;
    elem->set_message = NULL;
    elem->type = type;
    elem->active = 0;
}

