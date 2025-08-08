#include "clither/game/input.h"
#include "clither/ui/ui.h"
#include "clither/util/str.h"

/* ------------------------------------------------------------------------- */
void textinput_step_anim(
    struct ui_element* elem, const struct input* input, uint8_t sim_tick_rate)
{
    const int period = sim_tick_rate / 3;
    if (elem->u.textinput.blink_counter++ >= period)
    {
        elem->u.textinput.blink_counter = 0;
        elem->u.textinput.blink_on = !elem->u.textinput.blink_on;
    }
    (void)input;
}

/* ------------------------------------------------------------------------- */
enum ui_cmd_type textinput_interact(
    struct ui*         ui,
    union ui_cmd*      cmd,
    struct ui_element* elem,
    struct input*      input)
{
    const uint32_t* codepoint;
    vec_for_each (input->keys, codepoint)
        if (vec_count(elem->u.textinput.input_buffer) < 16 &&
            *codepoint != '\0' && *codepoint != '\n' && *codepoint != '\r')
        {
            codepoint_vec_push(&elem->u.textinput.input_buffer, *codepoint);
        }
    codepoint_vec_clear(input->keys);

    if (input->backspace)
    {
        if (vec_count(elem->u.textinput.input_buffer) > 0)
            codepoint_vec_pop(elem->u.textinput.input_buffer);
        input->backspace = 0;
    }

    str_set_utf32(
        &elem->u.textinput.input_buffer_utf8,
        vec_data(elem->u.textinput.input_buffer),
        vec_count(elem->u.textinput.input_buffer));
    str_set(
        &elem->u.textinput.text.str,
        str_view(elem->u.textinput.input_buffer_utf8));

    (void)ui, (void)cmd;
    return UI_CMD_NONE;
}

/* ------------------------------------------------------------------------- */
int ui_textinput_set_current_text(
    struct ui_element* elem, const char* current_text)
{
    struct ui_textinput* ti = &elem->u.textinput;
    const char*          p = current_text;
    for (; *p; ++p)
        if (codepoint_vec_push(&ti->input_buffer, (uint32_t)*p) != 0)
            return -1;
    if (str_set_cstr(&ti->input_buffer_utf8, current_text) != 0)
        return -1;
    return str_set_cstr(&ti->text.str, current_text);
}

/* ------------------------------------------------------------------------- */
struct ui_element ui_textinput(
    struct fpos pos, struct ui_text_style style, const char* current_text)
{
    struct ui_element elem;
    ui_element_init(&elem, UI_TEXTINPUT);

    elem.u.textinput.text =
        ui_text(cstr_view(""), pos, style, UI_ALIGN_LEFT, NULL).u.text;
    codepoint_vec_init(&elem.u.textinput.input_buffer);
    str_init(&elem.u.textinput.input_buffer_utf8);
    ui_textinput_set_current_text(&elem, current_text);

    elem.u.textinput.blink_counter = 0;
    elem.u.textinput.blink_on = 0;

    elem.step_anim = textinput_step_anim;
    elem.interact = textinput_interact;

    return elem;
}
