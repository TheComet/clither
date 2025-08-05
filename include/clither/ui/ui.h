#pragma once

#include "clither/game/fpos.h"
#include "clither/util/strview.h"
#include <stdint.h>

struct input;
struct ui;

enum ui_element_type
{
    UI_CONTROLLER,
    UI_RECTANGLE,
    UI_TEXT,
    UI_TEXTINPUT,
    UI_BUTTON,
    UI_SLIDER
};

enum ui_align
{
    UI_ALIGN_LEFT,
    UI_ALIGN_CENTER,
    UI_ALIGN_RIGHT
};

enum ui_cmd_type
{
    UI_CMD_NONE,
    UI_CMD_QUIT,
    UI_CMD_JOIN,
    UI_CMD_HOST,
    UI_CMD_GARAGE
};

enum ui_screen
{
    UI_MAIN_SCREEN_TITLE = 0,
    UI_MAIN_SCREEN_HOST,
    UI_MAIN_SCREEN_JOIN,
    UI_MAIN_SCREEN_HOST_ERROR,
    UI_MAIN_SCREEN_JOIN_ERROR
};

union ui_cmd
{
    struct
    {
        const char* username;
        const char* address;
        const char* port;
    } host;

    struct
    {
        const char* username;
        const char* address;
        const char* port;
    } join;
};

struct ui_rectangle
{
    struct fpos pos;
    struct fpos size;
    uint32_t    color;
};

struct ui_text_style
{
    uint32_t color;
    float    size;
};

struct ui_text
{
    struct str*   str;
    struct fpos   pos;
    uint32_t      color;
    float         size;
    enum ui_align align;
};

struct ui_textinput
{
    struct ui_text        text;
    struct codepoint_vec* input_buffer;
    struct str*           input_buffer_utf8;
    int                   blink_counter;
    unsigned              blink_on : 1;
};

struct ui_button_style
{
    uint32_t color;
    uint32_t hover_color;
    uint32_t disabled_color;
    float    text_size;
};

struct ui_button
{
    struct ui_text text;

    uint32_t normal_color;
    uint32_t hover_color;
    uint32_t disabled_color;
    int      hover_crossfade;
    unsigned enabled : 1;
    unsigned hover : 1;
    unsigned mouse_controlled : 1;
};

struct ui_slider_style
{
    uint32_t normal_color;
    uint32_t hover_color;
    float    knob_diameter;
};

struct ui_slider
{
    struct fpos start;
    struct fpos end;
    float       value; /* 0.0 - 1.0 */
    uint32_t    color;
    float       knob_diameter;

    uint32_t normal_color;
    uint32_t hover_color;
    int      hover_crossfade;
    unsigned knob_hover : 1;
    unsigned grabbed : 1;
};

struct ui_element
{
    int (*is_mouse_over)(struct ui_element* elem, const struct input* input);
    void (*step_anim)(
        struct ui_element*  elem,
        const struct input* input,
        uint8_t             sim_tick_rate);
    enum ui_cmd_type (*interact)(
        struct ui*         ui,
        union ui_cmd*      cmd,
        struct ui_element* elem,
        struct input*      input);
    void (*set_message)(struct ui_element* elem, const char* message);

    union
    {
        struct ui_rectangle rectangle;
        struct ui_text      text;
        struct ui_button    button;
        struct ui_textinput textinput;
        struct ui_slider    slider;
    } u;
    enum ui_element_type type;
    unsigned             active : 1;
};

struct ui
{
    const int**       screens;
    int               count;
    struct ui_element elements[1];
};

extern struct ui_button_style ui_style_button;
extern struct ui_text_style   ui_style_text_title;
extern struct ui_text_style   ui_style_text_subtitle;
extern struct ui_text_style   ui_style_text_subsubtitle;
extern struct ui_text_style   ui_style_text_normal;
extern struct ui_text_style   ui_style_text_small;
extern struct ui_slider_style ui_style_slider;

void ui_element_init(struct ui_element* elem, enum ui_element_type type);

struct ui_element ui_controller(enum ui_cmd_type (*interact)(
    struct ui*, union ui_cmd*, struct ui_element*, struct input*));

struct ui_element
ui_rectangle(struct fpos pos, struct fpos size, uint32_t color);

struct ui_text_style ui_text_style(uint32_t color, float size);

struct ui_element ui_text(
    struct strview       str,
    struct fpos          pos,
    struct ui_text_style style,
    enum ui_align        align,
    void (*set_message)(struct ui_element* elem, const char* message));
void ui_text_set_message(struct ui_element* elem, const char* message);

struct ui_element ui_textinput(struct fpos pos, struct ui_text_style style);

struct ui_element ui_button(
    struct strview         str,
    struct fpos            pos,
    struct ui_button_style style,
    int (*is_mouse_over)(struct ui_element*, const struct input*),
    enum ui_cmd_type (*interact)(
        struct ui*, union ui_cmd*, struct ui_element*, struct input*));

struct ui_element ui_slider(
    struct fpos            start,
    struct fpos            end,
    struct ui_slider_style style,
    enum ui_cmd_type (*interact)(
        struct ui*, union ui_cmd*, struct ui_element*, struct input*));

struct ui* ui_create(const int** screens, int count);
struct ui* ui_create_main_menu(void);
struct ui* ui_create_in_game(void);
void       ui_destroy(struct ui* ui);

void ui_deactivate_all(struct ui* ui);
void ui_switch_screen(struct ui* ui, enum ui_screen screen_idx);
void ui_set_message_on_active_screen(struct ui* ui, const char* message);

enum ui_cmd_type ui_update(
    struct ui*    ui,
    union ui_cmd* cmd,
    struct input* input,
    uint8_t       sim_tick_rate);

#define ui_for_each_active(ui, elem)                                           \
    for (elem = (ui)->elements; elem != ((ui)->elements + (ui)->count);        \
         ++elem)                                                               \
        if ((elem)->active)

#define ui_for_each(ui, elem)                                                  \
    for (elem = (ui)->elements; elem != ((ui)->elements + (ui)->count); ++elem)

#define check_and_clear(cond) ((cond) && ((cond) = 0, 1))
