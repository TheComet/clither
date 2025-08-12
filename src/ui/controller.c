#include "clither/ui/ui.h"

/* ------------------------------------------------------------------------- */
struct ui_element ui_controller(enum ui_cmd_type (*interact)(
    struct ui*,
    union ui_cmd*,
    struct ui_element*,
    struct input*,
    const struct audio_interface* iaudio,
    struct audio*                 audio))
{
    struct ui_element elem;
    ui_element_init(&elem, UI_CONTROLLER);
    elem.interact = interact;
    return elem;
}
