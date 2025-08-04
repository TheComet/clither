#include "clither/game/input.h"
#include "clither/ui/ui.h"
#include "clither/util/mem.h"
#include "clither/util/str.h"
#include <stddef.h>

/* ------------------------------------------------------------------------- */
void ui_deactivate_all(struct ui* ui)
{
    struct ui_element* elem;
    ui_for_each (ui, elem)
        elem->active = 0;
}

/* ------------------------------------------------------------------------- */
void ui_switch_screen(struct ui* ui, enum ui_screen screen_idx)
{
    int* screen;

    ui_deactivate_all(ui);

    screen = ui->screens[screen_idx];
    while (*screen != 0)
        ui->elements[*screen++].active = 1;
}

/* ------------------------------------------------------------------------- */
void ui_set_message_on_active_screen(struct ui* ui, const char* message)
{
    struct ui_element* elem;
    ui_for_each_active (ui, elem)
        if (elem->set_message != NULL)
            elem->set_message(elem, message);
}

/* ------------------------------------------------------------------------- */
struct ui* ui_create(int** screens, int count)
{
    struct ui* ui;
    int        header = offsetof(struct ui, elements);
    int        data = sizeof(struct ui_element) * count;

    ui = mem_alloc(header + data);
    if (ui == NULL)
        return NULL;
    memset(ui, 0x00, header + data);
    ui->screens = screens;
    ui->count = count;

    return ui;
}

/* ------------------------------------------------------------------------- */
struct ui* ui_create_in_game(void)
{
    return ui_create(NULL, 0);
}

/* ------------------------------------------------------------------------- */
void ui_destroy(struct ui* ui)
{
    struct ui_element* elem;
    ui_for_each (ui, elem)
    {
        switch (elem->type)
        {
            case UI_RECTANGLE: break;

            case UI_TEXT: {
                str_deinit(elem->u.text.str);
                break;
            }

            case UI_TEXTINPUT: {
                str_deinit(elem->u.textinput.text.str);
                str_deinit(elem->u.textinput.input_buffer_utf8);
                codepoint_vec_deinit(elem->u.textinput.input_buffer);
                break;
            }

            case UI_BUTTON: {
                str_deinit(elem->u.button.text.str);
                break;
            }

            case UI_CONTROLLER: break;
        }
    }

    mem_free(ui);
}

/* ------------------------------------------------------------------------- */
enum ui_cmd_type ui_update(
    struct ui*    ui,
    union ui_cmd* cmd,
    struct input* input,
    uint8_t       sim_tick_rate)
{
    struct ui_element* elem;
    ui_for_each_active (ui, elem)
        if (elem->step_anim)
            elem->step_anim(elem, input, sim_tick_rate);

    ui_for_each_active (ui, elem)
        if (elem->interact)
        {
            enum ui_cmd_type cmd_type = elem->interact(ui, cmd, elem, input);
            if (cmd_type != UI_CMD_NONE)
                return cmd_type;
        }

    return UI_CMD_NONE;
}
