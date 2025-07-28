#pragma once

#include "./background.h"
#include "./quad.h"
#include "./snake.h"
#include "./spine.h"
#include "./sprite.h"
#include "./sprite_shadow.h"
#include "./text.h"
#include "clither/game/input.h"

#if defined(CLITHER_GFX_DEBUG)
#    include "./debug.h"
#endif

#if defined(CLITHER_DEBUG_MEMORY)
void gfx_track_tex(GLuint tex);
void gfx_track_buf(GLuint buf);
void gfx_track_fbo(GLuint fbo);
void gfx_track_shader(GLuint shader);

void gfx_untrack_tex(GLuint tex);
void gfx_untrack_buf(GLuint buf);
void gfx_untrack_fbo(GLuint fbo);
void gfx_untrack_shader(GLuint shader);
#else
/* clang-format off */
#    define gfx_track_tex(tex) do {} while (0)
#    define gfx_track_buf(buf) do {} while (0)
#    define gfx_track_fbo(fbo) do {} while (0)
#    define gfx_track_shader(shader) do {} while (0)

#    define gfx_untrack_tex(tex) do {} while (0)
#    define gfx_untrack_buf(buf) do {} while (0)
#    define gfx_untrack_fbo(fbo) do {} while (0)
#    define gfx_untrack_shader(shader) do {} while (0)
/* clang-format on */
#endif

struct gfx
{
    struct GLFWwindow* window;
    int                width, height;

    struct input input_buffer;

    struct gfx_font       font;

    struct gfx_background        background;
    struct gfx_quad_mesh         quad_mesh;
    struct gfx_sprite_mat        sprite_mat;
    struct gfx_sprite_shadow_mat sprite_shadow_mat;
    struct gfx_snake             snake;
    struct gfx_sprite_tex        food;

#if defined(CLITHER_GFX_DEBUG)
    struct gfx_debug debug;
#endif
};

struct aspect_ratio
{
    float scale_x, scale_y;
    float pad_x, pad_y;
};
