#include "clither/game/input.h"
#include "clither/ui/ui.h"
#include "clither/util/str.h"

enum ui_element_index
{
    NONE,

    CONTROLLER_VIM,
    TEXT_TITLE,
    BUTTON_HOST,
    BUTTON_JOIN,
    BUTTON_GARAGE,
    BUTTON_QUIT,

    TEXT_ENTER_USERNAME,
    TEXT_HOST_GAME,
    TEXT_JOIN_GAME,
    TEXTEDIT_USERNAME,
    BUTTON_HOST_GAME,
    BUTTON_JOIN_GAME,
    BUTTON_BACK_TO_MAIN,

    TEXT_HOST_ERROR_TITLE,
    TEXT_HOST_ERROR_MESSAGE,
    BUTTON_BACK_TO_HOST,

    TEXT_JOIN_ERROR_TITLE,
    TEXT_JOIN_ERROR_MESSAGE,
    BUTTON_BACK_TO_JOIN,

    ELEMENT_COUNT
};

/* clang-format off */
static int title_screen[] = {
    CONTROLLER_VIM,
    TEXT_TITLE,
    BUTTON_HOST,
    BUTTON_JOIN,
    BUTTON_GARAGE,
    BUTTON_QUIT,
    0
};
static int host_screen[] = {
    TEXT_HOST_GAME,
    TEXT_ENTER_USERNAME,
    TEXTEDIT_USERNAME,
    BUTTON_HOST_GAME,
    BUTTON_BACK_TO_MAIN,
    0
};
static int join_screen[] = {
    TEXT_JOIN_GAME,
    TEXT_ENTER_USERNAME,
    TEXTEDIT_USERNAME,
    BUTTON_JOIN_GAME,
    BUTTON_BACK_TO_MAIN,
    0
};
static int host_error[] = {
    TEXT_HOST_ERROR_TITLE,
    TEXT_HOST_ERROR_MESSAGE,
    BUTTON_BACK_TO_HOST,
    0
};
static int join_error[] = {
    TEXT_JOIN_ERROR_TITLE,
    TEXT_JOIN_ERROR_MESSAGE,
    BUTTON_BACK_TO_JOIN,
    0
};
/* clang-format on */

static int* screens[] = {
    title_screen, host_screen, join_screen, host_error, join_error, NULL};

/* ------------------------------------------------------------------------- */
static union ui_cmd
make_ui_host_cmd(const char* username, const char* address, const char* port)
{
    union ui_cmd cmd;
    cmd.host.username = username;
    cmd.host.address = address;
    cmd.host.port = port;
    return cmd;
}

/* ------------------------------------------------------------------------- */
static union ui_cmd
make_ui_join_cmd(const char* username, const char* address, const char* port)
{
    union ui_cmd cmd;
    cmd.join.username = username;
    cmd.join.address = address;
    cmd.join.port = port;
    return cmd;
}

/* ------------------------------------------------------------------------- */
static int
button_is_mouse_over(struct ui_element* elem, const struct input* input)
{
    return input->mousex_ar >= elem->u.button.text.pos.x - 0.3 &&
           input->mousex_ar <= elem->u.button.text.pos.x + 0.3 &&
           input->mousey_ar >= elem->u.button.text.pos.y - 0.05 &&
           input->mousey_ar <= elem->u.button.text.pos.y + 0.15;
}
static int
button_is_mouse_over_smaller(struct ui_element* elem, const struct input* input)
{
    return input->mousex_ar >= elem->u.button.text.pos.x - 0.1 &&
           input->mousex_ar <= elem->u.button.text.pos.x + 0.1 &&
           input->mousey_ar >= elem->u.button.text.pos.y - 0.05 &&
           input->mousey_ar <= elem->u.button.text.pos.y + 0.15;
}

/* ------------------------------------------------------------------------- */
static enum ui_cmd_type button_host_interact(
    struct ui*         ui,
    union ui_cmd*      cmd,
    struct ui_element* elem,
    struct input*      input)
{
    if (elem->u.button.hover && (check_and_clear(input->screen_clicked) ||
                                 check_and_clear(input->enter)))
    {
        ui_switch_screen(ui, UI_MAIN_SCREEN_HOST);
    }
    (void)cmd;
    return UI_CMD_NONE;
}

/* ------------------------------------------------------------------------- */
static enum ui_cmd_type button_join_interact(
    struct ui*         ui,
    union ui_cmd*      cmd,
    struct ui_element* elem,
    struct input*      input)
{
    if (elem->u.button.hover && (check_and_clear(input->screen_clicked) ||
                                 check_and_clear(input->enter)))
    {
        ui_switch_screen(ui, UI_MAIN_SCREEN_JOIN);
    }
    (void)cmd;
    return UI_CMD_NONE;
}

/* ------------------------------------------------------------------------- */
static enum ui_cmd_type button_quit_interact(
    struct ui*         ui,
    union ui_cmd*      cmd,
    struct ui_element* elem,
    struct input*      input)
{
    if (elem->u.button.hover && (check_and_clear(input->screen_clicked) ||
                                 check_and_clear(input->enter)))
    {
        return UI_CMD_QUIT;
    }
    if (check_and_clear(input->escape))
        return UI_CMD_QUIT;

    (void)ui, (void)cmd;
    return UI_CMD_NONE;
}

/* ------------------------------------------------------------------------- */
static enum ui_cmd_type button_back_to_main_interact(
    struct ui*         ui,
    union ui_cmd*      cmd,
    struct ui_element* elem,
    struct input*      input)
{
    if (elem->u.button.hover && check_and_clear(input->screen_clicked))
        ui_switch_screen(ui, UI_MAIN_SCREEN_TITLE);
    if (check_and_clear(input->escape))
        ui_switch_screen(ui, UI_MAIN_SCREEN_TITLE);

    (void)cmd;
    return UI_CMD_NONE;
}

/* ------------------------------------------------------------------------- */
static enum ui_cmd_type button_back_to_host_interact(
    struct ui*         ui,
    union ui_cmd*      cmd,
    struct ui_element* elem,
    struct input*      input)
{
    if (elem->u.button.hover && check_and_clear(input->screen_clicked))
        ui_switch_screen(ui, UI_MAIN_SCREEN_HOST);
    if (check_and_clear(input->escape))
        ui_switch_screen(ui, UI_MAIN_SCREEN_HOST);

    (void)cmd;
    return UI_CMD_NONE;
}

/* ------------------------------------------------------------------------- */
static enum ui_cmd_type button_back_to_join_interact(
    struct ui*         ui,
    union ui_cmd*      cmd,
    struct ui_element* elem,
    struct input*      input)
{
    if (elem->u.button.hover && check_and_clear(input->screen_clicked))
        ui_switch_screen(ui, UI_MAIN_SCREEN_JOIN);
    if (check_and_clear(input->escape))
        ui_switch_screen(ui, UI_MAIN_SCREEN_JOIN);

    (void)cmd;
    return UI_CMD_NONE;
}

/* ------------------------------------------------------------------------- */
static enum ui_cmd_type button_host_game_interact(
    struct ui*         ui,
    union ui_cmd*      cmd,
    struct ui_element* elem,
    struct input*      input)
{
    const struct ui_textinput* textinput;
    textinput = &ui->elements[TEXTEDIT_USERNAME].u.textinput;

    elem->u.button.enabled = vec_count(textinput->input_buffer) > 0;
    if (!elem->u.button.enabled)
        return UI_CMD_NONE;

    if ((elem->u.button.hover && check_and_clear(input->screen_clicked)) ||
        check_and_clear(input->enter))
    {
        *cmd = make_ui_host_cmd(
            str_cstr(textinput->input_buffer_utf8), "0.0.0.0", "5555");
        return UI_CMD_HOST;
    }

    (void)cmd;
    return UI_CMD_NONE;
}

/* ------------------------------------------------------------------------- */
static enum ui_cmd_type button_join_game_interact(
    struct ui*         ui,
    union ui_cmd*      cmd,
    struct ui_element* elem,
    struct input*      input)
{
    const struct ui_textinput* textinput;
    textinput = &ui->elements[TEXTEDIT_USERNAME].u.textinput;

    elem->u.button.enabled = vec_count(textinput->input_buffer) > 0;
    if (!elem->u.button.enabled)
        return UI_CMD_NONE;

    if ((elem->u.button.hover && check_and_clear(input->screen_clicked)) ||
        check_and_clear(input->enter))
    {
        *cmd = make_ui_join_cmd(
            str_cstr(textinput->input_buffer_utf8), "localhost", "5555");
        return UI_CMD_JOIN;
    }

    return UI_CMD_NONE;
}

/* ------------------------------------------------------------------------- */
static enum ui_cmd_type controller_vim(
    struct ui*         ui,
    union ui_cmd*      cmd,
    struct ui_element* elem,
    struct input*      input)
{
    const uint32_t*       codepoint;
    int*                  idx;
    enum ui_element_index old_idx;

    enum direction
    {
        NONE,
        UP,
        DOWN
    } direction = NONE;

    if (input->mouse_moved)
        for (idx = title_screen; *idx != 0; ++idx)
            if (ui->elements[*idx].type == UI_BUTTON)
                ui->elements[*idx].u.button.mouse_controlled = 1;

    vec_for_each (input->keys, codepoint)
    {
        switch (*codepoint)
        {
            case 'j': direction = DOWN; break;
            case 'k': direction = UP; break;
            case 'q': break;
        }

        if (direction == NONE)
            continue;
        for (idx = title_screen; *idx != 0; ++idx)
            if (ui->elements[*idx].type == UI_BUTTON)
                ui->elements[*idx].u.button.mouse_controlled = 0;

        for (idx = title_screen; *idx != 0; ++idx)
            if (ui->elements[*idx].type == UI_BUTTON)
                if (ui->elements[*idx].u.button.hover)
                    break;
        old_idx = *idx;

        if (*idx == 0)
        {
            for (idx = title_screen; *idx != 0; ++idx)
                if (ui->elements[*idx].type == UI_BUTTON)
                {
                    ui->elements[*idx].u.button.hover = 1;
                    break;
                }
        }
        else
        {
            for (idx = direction == DOWN ? idx + 1 : idx - 1;
                 idx >= title_screen && *idx != 0;
                 idx = direction == DOWN ? idx + 1 : idx - 1)
            {
                if (ui->elements[*idx].type == UI_BUTTON &&
                    ui->elements[*idx].u.button.enabled)
                {
                    ui->elements[*idx].u.button.hover = 1;
                    ui->elements[old_idx].u.button.hover = 0;
                    break;
                }
            }
        }
    }

    (void)cmd, (void)elem;
    return UI_CMD_NONE;
}

/* ------------------------------------------------------------------------- */
struct ui* ui_create_main_menu(void)
{
    struct ui* ui = ui_create(screens, ELEMENT_COUNT);
    if (ui == NULL)
        return NULL;

    ui->elements[TEXT_TITLE] = ui_text(
        cstr_view("MechaSnek"),
        make_fpos(0.0, 0.6),
        ui_style_text_title,
        UI_ALIGN_CENTER,
        NULL);
    ui->elements[CONTROLLER_VIM] = ui_controller(controller_vim);
    ui->elements[BUTTON_HOST] = ui_button(
        cstr_view("Host"),
        make_fpos(0, 0.0),
        ui_style_button,
        button_is_mouse_over,
        button_host_interact);
    ui->elements[BUTTON_JOIN] = ui_button(
        cstr_view("Join"),
        make_fpos(0, -0.2),
        ui_style_button,
        button_is_mouse_over,
        button_join_interact);
    ui->elements[BUTTON_GARAGE] = ui_button(
        cstr_view("Garage"),
        make_fpos(0, -0.4),
        ui_style_button,
        button_is_mouse_over,
        NULL);
    ui->elements[BUTTON_QUIT] = ui_button(
        cstr_view("Quit"),
        make_fpos(0, -0.6),
        ui_style_button,
        button_is_mouse_over,
        button_quit_interact);

    ui->elements[TEXT_HOST_GAME] = ui_text(
        cstr_view("Host Game"),
        make_fpos(0.0, 0.4),
        ui_style_text_subtitle,
        UI_ALIGN_CENTER,
        NULL);
    ui->elements[TEXT_JOIN_GAME] = ui_text(
        cstr_view("Join Game"),
        make_fpos(0.0, 0.4),
        ui_style_text_subtitle,
        UI_ALIGN_CENTER,
        NULL);
    ui->elements[TEXT_ENTER_USERNAME] = ui_text(
        cstr_view("Enter username:"),
        make_fpos(-0.3, 0.0),
        ui_style_text_normal,
        UI_ALIGN_RIGHT,
        NULL);
    ui->elements[TEXTEDIT_USERNAME] =
        ui_textinput(make_fpos(-0.22, 0.0), ui_style_text_normal);
    ui->elements[BUTTON_HOST_GAME] = ui_button(
        cstr_view("Host"),
        make_fpos(0.4, -0.6),
        ui_style_button,
        button_is_mouse_over_smaller,
        button_host_game_interact);
    ui->elements[BUTTON_JOIN_GAME] = ui_button(
        cstr_view("Join"),
        make_fpos(0.4, -0.6),
        ui_style_button,
        button_is_mouse_over_smaller,
        button_join_game_interact);
    ui->elements[BUTTON_BACK_TO_MAIN] = ui_button(
        cstr_view("Back"),
        make_fpos(-0.4, -0.6),
        ui_style_button,
        button_is_mouse_over_smaller,
        button_back_to_main_interact);

    ui->elements[TEXT_HOST_ERROR_TITLE] = ui_text(
        cstr_view("Server Error"),
        make_fpos(0.0, 0.4),
        ui_style_text_subsubtitle,
        UI_ALIGN_CENTER,
        NULL);
    ui->elements[TEXT_HOST_ERROR_MESSAGE] = ui_text(
        cstr_view("Failed to host game"),
        make_fpos(0.0, 0.2),
        ui_style_text_normal,
        UI_ALIGN_CENTER,
        ui_text_set_message);
    ui->elements[BUTTON_BACK_TO_HOST] = ui_button(
        cstr_view("Back"),
        make_fpos(0.0, -0.6),
        ui_style_button,
        button_is_mouse_over_smaller,
        button_back_to_host_interact);

    ui->elements[TEXT_JOIN_ERROR_TITLE] = ui_text(
        cstr_view("Failed to Connect to Server"),
        make_fpos(0.0, 0.4),
        ui_style_text_subsubtitle,
        UI_ALIGN_CENTER,
        NULL);
    ui->elements[TEXT_JOIN_ERROR_MESSAGE] = ui_text(
        cstr_view("Failed to join game"),
        make_fpos(0.0, 0.2),
        ui_style_text_normal,
        UI_ALIGN_CENTER,
        ui_text_set_message);
    ui->elements[BUTTON_BACK_TO_JOIN] = ui_button(
        cstr_view("Back"),
        make_fpos(0.0, -0.6),
        ui_style_button,
        button_is_mouse_over_smaller,
        button_back_to_join_interact);

    return ui;
}
