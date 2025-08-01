#include "clither/game/input.h"
#include "clither/ui/ui.h"
#include "clither/util/mem.h"
#include "clither/util/str.h"
#include <stddef.h>

#define check_and_clear(cond) ((cond) && ((cond) = 0, 1))

enum ui_element_index
{
    BACKGROUND_FADER,

    TEXT_TITLE,
    BUTTON_HOST,
    BUTTON_JOIN,
    BUTTON_GARAGE,
    BUTTON_QUIT,

    TEXT_ENTER_USERNAME,
    TEXTEDIT_USERNAME,
    BUTTON_HOST_GAME,
    BUTTON_JOIN_GAME,
    BUTTON_BACK_TO_MAIN,

    ELEMENT_COUNT
};

static enum ui_element_index main_screen[] = {
    BACKGROUND_FADER,
    TEXT_TITLE,
    BUTTON_HOST,
    BUTTON_JOIN,
    BUTTON_GARAGE,
    BUTTON_QUIT,
    ELEMENT_COUNT};
static enum ui_element_index host_screen[] = {
    BACKGROUND_FADER,
    TEXT_ENTER_USERNAME,
    TEXTEDIT_USERNAME,
    BUTTON_HOST_GAME,
    BUTTON_BACK_TO_MAIN,
    ELEMENT_COUNT};
static enum ui_element_index join_screen[] = {
    BACKGROUND_FADER,
    TEXT_ENTER_USERNAME,
    TEXTEDIT_USERNAME,
    BUTTON_JOIN_GAME,
    BUTTON_BACK_TO_MAIN,
    ELEMENT_COUNT};

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
static void switch_screen(struct ui* ui, enum ui_element_index* screen)
{
    struct ui_element* elem;
    ui_for_each (ui, elem)
        elem->active = 0;
    while (*screen != ELEMENT_COUNT)
        ui->elements[*screen++].active = 1;
}

/* ------------------------------------------------------------------------- */
static void ui_element_init(struct ui_element* elem, enum ui_element_type type)
{
    elem->is_mouse_over = NULL;
    elem->step_anim = NULL;
    elem->interact = NULL;
    elem->type = type;
    elem->active = 0;
}

/* ------------------------------------------------------------------------- */
static struct ui_element
make_ui_rectangle(struct fpos pos, struct fpos size, uint32_t color)
{
    struct ui_element elem;
    ui_element_init(&elem, UI_RECTANGLE);
    elem.u.rectangle.pos = pos;
    elem.u.rectangle.size = size;
    elem.u.rectangle.color = color;
    return elem;
}

/* ------------------------------------------------------------------------- */
static struct ui_element make_ui_text(
    struct strview str,
    struct fpos    pos,
    uint32_t       color,
    float          size,
    enum ui_align  align)
{
    struct ui_element elem;
    ui_element_init(&elem, UI_TEXT);
    elem.u.text.str = str;
    elem.u.text.pos = pos;
    elem.u.text.color = color;
    elem.u.text.size = size;
    elem.u.text.align = align;
    return elem;
}

/* ------------------------------------------------------------------------- */
static void textinput_step_anim(
    struct ui_element* elem, const struct input* input, uint8_t sim_tick_rate)
{
    const int period = sim_tick_rate / 4;
    if (elem->u.textinput.blink_counter++ >= period)
    {
        elem->u.textinput.blink_counter = 0;
        elem->u.textinput.blink_on = !elem->u.textinput.blink_on;
    }
    (void)input;
}
static void
textinput_interact(struct ui* ui, struct ui_element* elem, struct input* input)
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
    elem->u.textinput.text.str = str_view(elem->u.textinput.input_buffer_utf8);

    (void)ui;
}
static struct ui_element
make_ui_textinput(struct fpos pos, uint32_t color, float size)
{
    struct ui_element elem;
    ui_element_init(&elem, UI_TEXTINPUT);

    elem.u.textinput.text =
        make_ui_text(cstr_view("TheComet"), pos, color, size, UI_ALIGN_LEFT)
            .u.text;
    codepoint_vec_init(&elem.u.textinput.input_buffer);
    str_init(&elem.u.textinput.input_buffer_utf8);
    elem.u.textinput.blink_counter = 0;
    elem.u.textinput.blink_on = 0;

    elem.step_anim = textinput_step_anim;
    elem.interact = textinput_interact;

    return elem;
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
static void button_step_anim(
    struct ui_element* elem, const struct input* input, uint8_t sim_tick_rate)
{
    const int crossfade_speed = sim_tick_rate / 12;
    if (elem->is_mouse_over == NULL)
        return;

    if (elem->is_mouse_over(elem, input))
    {
        if (elem->u.button.mouseover_crossfade < crossfade_speed)
            elem->u.button.mouseover_crossfade++;
    }
    else
    {
        if (elem->u.button.mouseover_crossfade > 0)
            elem->u.button.mouseover_crossfade--;
    }

    elem->u.button.color = crossfade_color(
        elem->u.button.text.color,
        elem->u.button.mouseover_color,
        elem->u.button.mouseover_crossfade,
        crossfade_speed);
}
static void button_host_interact(
    struct ui* ui, struct ui_element* elem, struct input* input)
{
    int mouse_over = elem->is_mouse_over(elem, input);
    if (mouse_over && check_and_clear(input->screen_clicked))
        switch_screen(ui, host_screen);
}
static void button_join_interact(
    struct ui* ui, struct ui_element* elem, struct input* input)
{
    int mouse_over = elem->is_mouse_over(elem, input);
    if (mouse_over && check_and_clear(input->screen_clicked))
        switch_screen(ui, join_screen);
}
static void button_quit_interact(
    struct ui* ui, struct ui_element* elem, struct input* input)
{
    int mouse_over = elem->is_mouse_over(elem, input);
    if (mouse_over && check_and_clear(input->screen_clicked))
        input->quit = 1;
    if (check_and_clear(input->escape))
        input->quit = 1;

    (void)ui;
}
static void button_back_to_main_interact(
    struct ui* ui, struct ui_element* elem, struct input* input)
{
    int mouse_over = elem->is_mouse_over && elem->is_mouse_over(elem, input);
    if (mouse_over && check_and_clear(input->screen_clicked))
        switch_screen(ui, main_screen);
    if (check_and_clear(input->escape))
        switch_screen(ui, main_screen);
}
static struct ui_element make_ui_button(
    struct strview str,
    struct fpos    pos,
    uint32_t       color,
    uint32_t       mouseover_color,
    float          text_size,
    int (*is_mouse_over)(struct ui_element*, const struct input*),
    void (*interact)(struct ui*, struct ui_element*, struct input*))
{
    struct ui_element elem;
    ui_element_init(&elem, UI_BUTTON);
    elem.u.button.text =
        make_ui_text(str, pos, color, text_size, UI_ALIGN_CENTER).u.text;
    elem.u.button.color = color;
    elem.u.button.mouseover_color = mouseover_color;
    elem.u.button.mouseover_crossfade = 0;

    elem.is_mouse_over = is_mouse_over;
    elem.step_anim = button_step_anim;
    elem.interact = interact;

    return elem;
}

/* ------------------------------------------------------------------------- */
struct ui* ui_create(void)
{
    int        header = offsetof(struct ui, elements);
    int        data = sizeof(struct ui_element) * ELEMENT_COUNT;
    struct ui* ui = mem_alloc(header + data);
    if (ui == NULL)
        return NULL;
    ui->count = ELEMENT_COUNT;

    ui->elements[BACKGROUND_FADER] =
        make_ui_rectangle(make_fpos(0, 0), make_fpos(1000, 1000), 0xE0000000);

    ui->elements[TEXT_TITLE] = make_ui_text(
        cstr_view("MechaSnek"),
        make_fpos(0.0, 0.6),
        0xA0FFFFFF,
        1.0 / 14,
        UI_ALIGN_CENTER);

    ui->elements[BUTTON_HOST] = make_ui_button(
        cstr_view("Host"),
        make_fpos(0, 0.0),
        0xA0FFFFFF,
        0xA0FF78FF,
        1.0 / 24,
        button_is_mouse_over,
        button_host_interact);
    ui->elements[BUTTON_JOIN] = make_ui_button(
        cstr_view("Join"),
        make_fpos(0, -0.2),
        0xA0FFFFFF,
        0xA0FF78FF,
        1.0 / 24,
        button_is_mouse_over,
        button_join_interact);
    ui->elements[BUTTON_GARAGE] = make_ui_button(
        cstr_view("Garage"),
        make_fpos(0, -0.4),
        0xA0FFFFFF,
        0xA0FF78FF,
        1.0 / 24,
        button_is_mouse_over,
        NULL);
    ui->elements[BUTTON_QUIT] = make_ui_button(
        cstr_view("Quit"),
        make_fpos(0, -0.6),
        0xA0FFFFFF,
        0xA0FF78FF,
        1.0 / 24,
        button_is_mouse_over,
        button_quit_interact);

    ui->elements[TEXT_ENTER_USERNAME] = make_ui_text(
        cstr_view("Enter username:"),
        make_fpos(-0.3, 0.0),
        0xA0FFFFFF,
        1.0 / 64,
        UI_ALIGN_RIGHT);
    ui->elements[TEXTEDIT_USERNAME] =
        make_ui_textinput(make_fpos(-0.26, 0.0), 0xA0FFFFFF, 1.0 / 64);
    ui->elements[BUTTON_HOST_GAME] = make_ui_button(
        cstr_view("Host"),
        make_fpos(0.2, -0.2),
        0xA0FFFFFF,
        0xA0FF78FF,
        1.0 / 36,
        button_is_mouse_over_smaller,
        NULL);
    ui->elements[BUTTON_JOIN_GAME] = make_ui_button(
        cstr_view("Join"),
        make_fpos(0.2, -0.2),
        0xA0FFFFFF,
        0xA0FF78FF,
        1.0 / 36,
        button_is_mouse_over_smaller,
        NULL);
    ui->elements[BUTTON_BACK_TO_MAIN] = make_ui_button(
        cstr_view("Back"),
        make_fpos(-0.2, -0.2),
        0xA0FFFFFF,
        0xA0FF78FF,
        1.0 / 36,
        button_is_mouse_over_smaller,
        button_back_to_main_interact);

    switch_screen(ui, main_screen);

    return ui;
}

/* ------------------------------------------------------------------------- */
void ui_destroy(struct ui* ui)
{
    struct ui_element* elem;
    ui_for_each (ui, elem)
    {
        if (elem->type == UI_TEXTINPUT)
        {
            str_deinit(elem->u.textinput.input_buffer_utf8);
            codepoint_vec_deinit(elem->u.textinput.input_buffer);
        }
    }

    mem_free(ui);
}

/* ------------------------------------------------------------------------- */
void ui_update(struct ui* ui, struct input* input, uint8_t sim_tick_rate)
{
    struct ui_element* elem;
    ui_for_each_active (ui, elem)
    {
        if (elem->step_anim)
            elem->step_anim(elem, input, sim_tick_rate);

        if (elem->interact)
            elem->interact(ui, elem, input);
    }
}
