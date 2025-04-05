#pragma once

#include "./background.h"
#include "./quad.h"
#include "./sprite.h"
#include "./sprite_shadow.h"
#include "./text.h"
#include "clither/game/input.h"

#if defined(CLITHER_GFX_DEBUG)
#    include "./debug.h"
#endif

struct gfx
{
    struct GLFWwindow* window;
    int                width, height;

    FT_Library  ft_lib;
    struct font font;
    struct text text;

    struct input input_buffer;

    struct background        background;
    struct quad_mesh         quad_mesh;
    struct sprite_mat        sprite_mat;
    struct sprite_shadow_mat sprite_shadow_mat;
    struct sprite_tex        food;
    struct sprite_tex        head0_base;
    struct sprite_tex        head0_gather;
    struct sprite_tex        body0_base;
    struct sprite_tex        tail0_base;
#if defined(CLITHER_GFX_DEBUG)
    struct debug debug;
#endif
};

struct aspect_ratio
{
    float scale_x, scale_y;
    float pad_x, pad_y;
};
