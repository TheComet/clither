#include "clither/client/client.h"
#include "clither/game/input.h"
#include "clither/game/resource_pack.h"
#include "clither/game/settings.h"
#include "clither/gfx/gfx.h"
#include "clither/platform/fs.h"
#include "clither/platform/signals.h"
#include "clither/platform/tick.h"
#include "clither/server/server_instance.h"
#include "clither/ui/ui.h"
#include "clither/util/cli_colors.h"
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
static enum ui_cmd_type button_garage_interact(
    struct ui*         ui,
    union ui_cmd*      cmd,
    struct ui_element* elem,
    struct input*      input)
{
    if (elem->u.button.hover && (check_and_clear(input->screen_clicked) ||
                                 check_and_clear(input->enter)))
    {
        return UI_CMD_GARAGE;
    }
    (void)ui, (void)cmd;
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
        button_garage_interact);
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

/* ------------------------------------------------------------------------- */
int main_menu_run(
    const struct gfx_interface** igfx,
    struct gfx**                 gfx,
    struct resource_pack**       pack,
    struct fs_watch**            pack_watch,
    const struct bot_interface*  ibot,
    struct bot*                  bot,
    const struct settings*       settings)
{
    struct ui*    main_menu;
    struct input  input;
    struct tick   sim_tick;
    union ui_cmd  ui_cmd;
    int           retval = -1;
    const uint8_t sim_tick_rate = 60;

    /* Change log prefix and color for server log messages */
    log_set_prefix(settings->client.log_prefix);
    log_set_colors(COL_B_GREEN, COL_RESET);

    main_menu = ui_create_main_menu();
    if (main_menu == NULL)
        goto create_main_menu_failed;
    ui_switch_screen(main_menu, UI_MAIN_SCREEN_TITLE);

    input_init(&input);

    tick_cfg(&sim_tick, sim_tick_rate);
    while (1)
    {
        (*igfx)->poll_input(*gfx, &input);

        switch (ui_update(main_menu, &ui_cmd, &input, sim_tick_rate))
        {
            case UI_CMD_NONE: break;

            case UI_CMD_QUIT: {
                input.quit = 1;
                break;
            }

            case UI_CMD_JOIN: {
                struct client client;
                client_init(&client);
                if (client_connect(
                        &client,
                        ui_cmd.join.address,
                        ui_cmd.join.port,
                        ui_cmd.join.username) != 0)
                {
                    ui_switch_screen(main_menu, UI_MAIN_SCREEN_JOIN_ERROR);
                    ui_set_message_on_active_screen(
                        main_menu, "Failed to connect to server");
                }
                if (client_run(
                        &client,
                        settings,
                        igfx,
                        gfx,
                        pack,
                        pack_watch,
                        ibot,
                        bot) != 0)
                {
                    ui_switch_screen(main_menu, UI_MAIN_SCREEN_JOIN_ERROR);
                    ui_set_message_on_active_screen(
                        main_menu, "Failed to connect to server");
                }
                client_deinit(&client);
                break;
            }

            case UI_CMD_HOST: {
                struct server_instance server;
                struct client          client;

                server_instance_init(&server);
                client_init(&client);

                log_dbg("Starting server in background thread\n");
                if (server_instance_start(
                        &server,
                        settings,
                        ui_cmd.host.address,
                        ui_cmd.host.port) != 0)
                {
                    goto start_server_failed;
                }

                server_instance_wait_for_ready(&server);

                /* The server should be running, so try to join as a client */
                if (client_connect(
                        &client,
                        "localhost",
                        ui_cmd.host.port,
                        ui_cmd.host.username) != 0)
                {
                    ui_switch_screen(main_menu, UI_MAIN_SCREEN_HOST_ERROR);
                    ui_set_message_on_active_screen(
                        main_menu, "Failed to connect to server");
                    goto client_connect_failed;
                }
                if (client_run(
                        &client,
                        settings,
                        igfx,
                        gfx,
                        pack,
                        pack_watch,
                        ibot,
                        bot) != 0)
                {
                    ui_switch_screen(main_menu, UI_MAIN_SCREEN_JOIN_ERROR);
                    ui_set_message_on_active_screen(
                        main_menu, "Failed to run client");
                }

            client_connect_failed:
                server_instance_stop(&server);
            start_server_failed:
                client_deinit(&client);
                break;
            }

            case UI_CMD_GARAGE: break; ;
        }

        /* CTRL+C */
        if (signals_exit_requested())
            input.quit = 1;

        /* Window close event */
        if (input.quit)
        {
            retval = 0;
            break;
        }

        /* Switch graphics backends */
        if (*gfx != NULL && input.next_gfx_backend)
        {
            gfx_next_backend(igfx, gfx, *pack);
            input.next_gfx_backend = 0;
        }
        if (*gfx != NULL && input.prev_gfx_backend)
        {
            gfx_prev_backend(igfx, gfx, *pack);
            input.prev_gfx_backend = 0;
        }

        /* Check for resource pack changes */
        if (*pack_watch != NULL && fs_watch_check(*pack_watch) > 0)
        {
            struct resource_pack* new_pack;
            log_info("Resource pack changed, reloading\n");

            new_pack = resource_pack_parse((*pack)->path);
            if (new_pack)
            {
                if (*gfx != NULL)
                {
                    (*igfx)->unload_resource_pack(*gfx, *pack);
                    (*igfx)->load_resource_pack(*gfx, new_pack);
                }

                resource_pack_destroy(*pack);
                *pack = new_pack;
            }

            fs_watch_deinit(*pack_watch);
            *pack_watch = resource_pack_watch_create(*pack);
        }

        if (*gfx != NULL)
        {
            (*igfx)->step_anim(*gfx, sim_tick_rate);
            (*igfx)->draw_begin(*gfx);
            (*igfx)->draw_ui(*gfx, main_menu);
            (*igfx)->draw_end(*gfx);
        }

        tick_wait(&sim_tick);
    }

    input_deinit(&input);
    ui_destroy(main_menu);
create_main_menu_failed:
    log_set_prefix("");
    log_set_colors("", "");
    return retval;
}
