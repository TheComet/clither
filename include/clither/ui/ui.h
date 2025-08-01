#pragma once

#include "clither/game/fpos.h"
#include "clither/util/strview.h"
#include <stdint.h>

struct input;
struct ui;

enum ui_element_type
{
    UI_RECTANGLE,
    UI_TEXT,
    UI_TEXTINPUT,
    UI_BUTTON,
    UI_CONTROLLER
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
    UI_CMD_HOST
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
    struct strview str;
    struct fpos    pos;
    uint32_t       color;
    float          size;
    enum ui_align  align;
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
    uint32_t mouseover_color;
    uint32_t disabled_color;
    float    text_size;
};

struct ui_button
{
    struct ui_text text;
    uint32_t       normal_color;
    uint32_t       mouseover_color;
    uint32_t       disabled_color;
    int            hover_crossfade;
    unsigned       enabled : 1;
    unsigned       hover : 1;
    unsigned       mouse_controlled : 1;
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

    union
    {
        struct ui_rectangle rectangle;
        struct ui_text      text;
        struct ui_button    button;
        struct ui_textinput textinput;
    } u;
    enum ui_element_type type;
    unsigned             active : 1;
};

struct ui
{
    int               count;
    struct ui_element elements[1];
};

struct ui* ui_create_main_menu(void);
struct ui* ui_create_in_game(void);
void       ui_destroy(struct ui* ui);

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
