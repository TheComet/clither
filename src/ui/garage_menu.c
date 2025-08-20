#include "clither/audio/audio.h"
#include "clither/client/client.h"
#include "clither/game/camera.h"
#include "clither/game/input.h"
#include "clither/game/math.h"
#include "clither/game/resource_pack.h"
#include "clither/game/settings.h"
#include "clither/game/snake_bmap.h"
#include "clither/game/world.h"
#include "clither/gfx/gfx.h"
#include "clither/platform/fs.h"
#include "clither/platform/signals.h"
#include "clither/platform/tick.h"
#include "clither/server/server_instance.h"
#include "clither/ui/garage_menu.h"
#include "clither/ui/ui.h"
#include "clither/util/cli_colors.h"

enum ui_element_index
{
    NONE,

    TEXT_SPACING,
    TEXT_SPINE_WIDTH,
    TEXT_HEAD_SCALE,
    TEXT_BODY_SCALE,
    TEXT_TAIL_SCALE,
    TEXT_GIRTH,
    TEXT_DECAY,
    SLIDER_PART_SPACING,
    SLIDER_SPINE_WIDTH,
    SLIDER_HEAD_SCALE,
    SLIDER_BODY_SCALE,
    SLIDER_TAIL_SCALE,
    SLIDER_GIRTH,
    SLIDER_DECAY,

    BUTTON_BACK_TO_MAIN,

    ELEMENT_COUNT
};

/* clang-format off */
static const int garage_screen[] = {
    TEXT_SPACING,
    TEXT_SPINE_WIDTH,
    TEXT_HEAD_SCALE,
    TEXT_BODY_SCALE,
    TEXT_TAIL_SCALE,
    TEXT_GIRTH,
    TEXT_DECAY,
    SLIDER_PART_SPACING,
    SLIDER_SPINE_WIDTH,
    SLIDER_HEAD_SCALE,
    SLIDER_BODY_SCALE,
    SLIDER_TAIL_SCALE,
    SLIDER_GIRTH,
    SLIDER_DECAY,

    BUTTON_BACK_TO_MAIN,
    0
};
/* clang-format on */

static const int* screens[] = {garage_screen, NULL};

/* ------------------------------------------------------------------------- */
static int
button_is_mouse_over(struct ui_element* elem, const struct input* input)
{
    return input->mousex_ar >= elem->u.button.text.pos.x - 0.15 &&
           input->mousex_ar <= elem->u.button.text.pos.x + 0.15 &&
           input->mousey_ar >= elem->u.button.text.pos.y - 0.05 &&
           input->mousey_ar <= elem->u.button.text.pos.y + 0.15;
}

/* ------------------------------------------------------------------------- */
static enum ui_cmd_type
create_snake_properties_command(const struct ui* ui, union ui_cmd* cmd)
{
#define X(name, NAME, def, min, max)                                           \
    cmd->snake_cosmetic_params.name =                                          \
        ui->elements[SLIDER_##NAME].u.slider.value;
    SNAKE_COSMETIC_PARAMS_LIST
#undef X

    return UI_CMD_SNAKE_COSMETIC_PARAMS;
}

/* ------------------------------------------------------------------------- */
static enum ui_cmd_type slider_snake_properties_interact(
    struct ui*                    ui,
    union ui_cmd*                 cmd,
    struct ui_element*            elem,
    struct input*                 input,
    const struct audio_interface* iaudio,
    struct audio*                 audio)
{
    if (!elem->u.slider.grabbed && elem->u.slider.knob_hover &&
        check_and_clear(input->screen_clicked))
    {
        elem->u.slider.grabbed = 1;
        if (iaudio != NULL)
            iaudio->play_sound(audio, SFX_SLIDER_CLICK);
    }

    if (elem->u.slider.grabbed && !input->mouse_down)
    {
        elem->u.slider.grabbed = 0;
        if (iaudio != NULL)
            iaudio->play_sound(audio, SFX_SLIDER_RELEASE);
    }

    if (elem->u.slider.grabbed)
    {
        float x1 = elem->u.slider.start.x;
        float x2 = elem->u.slider.end.x;
        float y1 = elem->u.slider.start.y;
        float y2 = elem->u.slider.end.y;
        float value = y1 == y2 ? (input->mousex_ar - x1) / (x2 - x1)
                               : (input->mousey_ar - y1) / (y2 - y1);
        elem->u.slider.value = value < 0.0 ? 0.0 : value > 1.0 ? 1.0 : value;
        return create_snake_properties_command(ui, cmd);
    }

    return UI_CMD_NONE;
}

/* ------------------------------------------------------------------------- */
static enum ui_cmd_type button_back_to_main_interact(
    struct ui*                    ui,
    union ui_cmd*                 cmd,
    struct ui_element*            elem,
    struct input*                 input,
    const struct audio_interface* iaudio,
    struct audio*                 audio)
{
    if ((elem->u.button.hover && check_and_clear(input->screen_clicked)) ||
        check_and_clear(input->escape))
    {
        if (iaudio != NULL)
            iaudio->play_sound(audio, SFX_BUTTON_BACK);
        return UI_CMD_QUIT;
    }

    (void)ui, (void)cmd;
    return UI_CMD_NONE;
}

/* ------------------------------------------------------------------------- */
struct ui* ui_create_garage_menu(const struct settings_snake* settings)
{
    struct ui* ui = ui_create(screens, ELEMENT_COUNT);
    if (ui == NULL)
        return NULL;

    ui->elements[TEXT_SPACING] = ui_text(
        cstr_view("Spacing"),
        make_fpos(.65, .9),
        ui_style_text_small,
        UI_ALIGN_RIGHT,
        NULL);
    ui->elements[TEXT_SPINE_WIDTH] = ui_text(
        cstr_view("Spine"),
        make_fpos(.65, .85),
        ui_style_text_small,
        UI_ALIGN_RIGHT,
        NULL);
    ui->elements[TEXT_HEAD_SCALE] = ui_text(
        cstr_view("Head size"),
        make_fpos(.65, .8),
        ui_style_text_small,
        UI_ALIGN_RIGHT,
        NULL);
    ui->elements[TEXT_BODY_SCALE] = ui_text(
        cstr_view("Body size"),
        make_fpos(.65, .75),
        ui_style_text_small,
        UI_ALIGN_RIGHT,
        NULL);
    ui->elements[TEXT_TAIL_SCALE] = ui_text(
        cstr_view("Tail size"),
        make_fpos(.65, .70),
        ui_style_text_small,
        UI_ALIGN_RIGHT,
        NULL);
    ui->elements[TEXT_GIRTH] = ui_text(
        cstr_view("Girth"),
        make_fpos(.65, .65),
        ui_style_text_small,
        UI_ALIGN_RIGHT,
        NULL);
    ui->elements[TEXT_DECAY] = ui_text(
        cstr_view("Decay"),
        make_fpos(.65, .6),
        ui_style_text_small,
        UI_ALIGN_RIGHT,
        NULL);
    ui->elements[SLIDER_PART_SPACING] = ui_slider(
        make_fpos(.7, .91),
        make_fpos(.95, .91),
        ui_style_slider,
        slider_snake_properties_interact);
    ui->elements[SLIDER_SPINE_WIDTH] = ui_slider(
        make_fpos(.7, .86),
        make_fpos(.95, .86),
        ui_style_slider,
        slider_snake_properties_interact);
    ui->elements[SLIDER_HEAD_SCALE] = ui_slider(
        make_fpos(.7, .81),
        make_fpos(.95, .81),
        ui_style_slider,
        slider_snake_properties_interact);
    ui->elements[SLIDER_BODY_SCALE] = ui_slider(
        make_fpos(.7, .76),
        make_fpos(.95, .76),
        ui_style_slider,
        slider_snake_properties_interact);
    ui->elements[SLIDER_TAIL_SCALE] = ui_slider(
        make_fpos(.7, .71),
        make_fpos(.95, .71),
        ui_style_slider,
        slider_snake_properties_interact);
    ui->elements[SLIDER_GIRTH] = ui_slider(
        make_fpos(.7, .66),
        make_fpos(.95, .66),
        ui_style_slider,
        slider_snake_properties_interact);
    ui->elements[SLIDER_DECAY] = ui_slider(
        make_fpos(.7, .61),
        make_fpos(.95, .61),
        ui_style_slider,
        slider_snake_properties_interact);
    ui->elements[BUTTON_BACK_TO_MAIN] = ui_button(
        cstr_view("Back"),
        make_fpos(-.75, -.9),
        ui_style_button,
        button_is_mouse_over,
        button_back_to_main_interact);

#define X(name, NAME, def, min, max)                                           \
    ui->elements[SLIDER_##NAME].u.slider.value =                               \
        unlerp(min, max, settings->name);
    SNAKE_COSMETIC_PARAMS_LIST
#undef X

    return ui;
}

/* ------------------------------------------------------------------------- */
int garage_menu_run(
    const struct audio_interface* iaudio,
    struct audio*                 audio,
    const struct gfx_interface**  igfx,
    struct gfx**                  gfx,
    struct resource_pack**        pack,
    struct fs_watch**             pack_watch,
    struct settings*              settings)
{
    struct ui*        garage_menu;
    struct world      world;
    struct snake*     snake;
    struct snake_head head;
    struct input      input;
    struct cmd        cmd;
    struct camera     camera;
    struct tick       sim_tick;
    union ui_cmd      ui_cmd;
    uint16_t          snake_id;
    const uint8_t     sim_tick_rate = 60;
    int               keep_running = 1;
    int               cmd_direction = 1;
    float             cmd_angle = 0.0f;

    /* Change log prefix and color for server log messages */
    log_set_prefix(settings->client.log_prefix);
    log_set_colors(COL_B_GREEN, COL_RESET);

    garage_menu = ui_create_garage_menu(&settings->snake);
    if (garage_menu == NULL)
        goto create_main_menu_failed;
    ui_switch_screen(garage_menu, UI_MAIN_SCREEN_TITLE);

    input_init(&input);
    camera_init(&camera);
    world_init(&world);

    snake_id = world_spawn_snake(&world, "");
    if (snake_id == WORLD_SPAWN_SNAKE_FAILED)
        goto spawn_snake_failed;
    snake = snake_bmap_find(world.snakes, snake_id);
    snake_param_apply_settings(&snake->param, &settings->snake);
    cmd = cmd_default();

    tick_cfg(&sim_tick, sim_tick_rate);
    while (keep_running)
    {
        (*igfx)->poll_input(*gfx, &input);

        switch (ui_update(
            iaudio, audio, garage_menu, &ui_cmd, &input, sim_tick_rate))
        {
            case UI_CMD_NONE: break;
            case UI_CMD_QUIT: {
                keep_running = 0;
                break;
            }
            case UI_CMD_JOIN: break;
            case UI_CMD_HOST: break;
            case UI_CMD_GARAGE: break;
            case UI_CMD_SNAKE_COSMETIC_PARAMS: {
#define X(name, NAME, def, min, max)                                           \
    settings->snake.name = lerp(min, max, ui_cmd.snake_cosmetic_params.name);
                SNAKE_COSMETIC_PARAMS_LIST
#undef X
                snake_param_apply_settings(&snake->param, &settings->snake);
                break;
            }
        }

        /* CTRL+C */
        if (signals_exit_requested())
            input.quit = 1;

        /* Window close event */
        if (input.quit)
            break;

        /* Switch graphics backends */
        if (*gfx != NULL && input.next_gfx_backend)
        {
            gfx_next_backend(igfx, gfx, *pack);
            input.next_gfx_backend = 0;
        }

        /* Check for resource pack changes */
#if defined(CLITHER_HOT_RELOAD)
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
#endif

        cmd_angle += cmd_direction * 0.008f;
        cmd = cmd_make(cmd, cmd_angle, 1.0, CMD_ACTION_NONE);
        if (cmd_angle > 0.3f || cmd_angle < -0.3f)
        {
            cmd_direction *= -1;
        }

        snake_remove_stale_segments(
            &snake->data,
            snake_step(
                &snake->data, &snake->head, &snake->param, cmd, sim_tick_rate));

        head = snake->head;
        head.pos = make_qwposqw(qw_sub(head.pos.x, make_qw(1.5)), head.pos.y);
        camera_update(&camera, &head, &snake->param, &input, sim_tick_rate);

        if (iaudio != NULL)
            iaudio->update(audio);

        if (*gfx != NULL)
        {
            (*igfx)->step_anim(*gfx, sim_tick_rate);
            (*igfx)->draw_begin(*gfx);
            (*igfx)->draw_world(*gfx, &world, &camera);
            (*igfx)->draw_ui(*gfx, garage_menu);
            (*igfx)->draw_end(*gfx);
        }

        tick_wait(&sim_tick);
    }

spawn_snake_failed:
    world_deinit(&world);
    input_deinit(&input);
    ui_destroy(garage_menu);
create_main_menu_failed:
    log_set_prefix("");
    log_set_colors("", "");
    return 0;
}
