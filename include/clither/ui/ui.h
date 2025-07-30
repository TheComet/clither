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
    UI_BUTTON
};

struct ui_rectangle
{
    struct fpos pos;
    struct fpos size;
    uint32_t    color;
};

struct ui_text
{
    struct strview str;
    struct fpos    pos;
    uint32_t       color;
    float          size;
};

struct ui_button
{
    struct ui_text text;
    uint32_t       color;
    uint32_t       mouseover_color;
    int            mouseover_crossfade;
};

struct ui_element
{
    int (*is_mouse_over)(struct ui_element* elem, const struct input* input);
    void (*step_anim)(
        struct ui_element*  elem,
        const struct input* input,
        uint8_t             sim_tick_rate);
    void (*interact)(
        struct ui* ui, struct ui_element* elem, const struct input* input);

    union
    {
        struct ui_rectangle rectangle;
        struct ui_text      text;
        struct ui_button    button;
    } u;
    enum ui_element_type type;
    unsigned             active : 1;
};

struct ui
{
    int               count;
    struct ui_element elements[1];
};

struct ui* ui_create(void);
void       ui_destroy(struct ui* ui);

void ui_update(struct ui* ui, const struct input* input, uint8_t sim_tick_rate);

#define ui_for_each(ui, elem)                                                  \
    for (elem = (ui)->elements; elem != ((ui)->elements + (ui)->count);        \
         ++elem)                                                               \
        if ((elem)->active)
