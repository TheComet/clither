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

#if defined(CLITHER_DEBUG_MEMORY)
#    define GFX_TRACKER(gfx) (&(gfx)->tracker)
struct tracker;
struct gfx_tracker
{
    struct tracker* tex;
    struct tracker* buf;
    struct tracker* fbo;
    struct tracker* shader;
};
void gfx_track_tex(struct gfx_tracker* tracker, GLuint tex);
void gfx_track_buf(struct gfx_tracker* tracker, GLuint buf);
void gfx_track_fbo(struct gfx_tracker* tracker, GLuint fbo);
void gfx_track_shader(struct gfx_tracker* tracker, GLuint shader);

void gfx_untrack_tex(struct gfx_tracker* tracker, GLuint tex);
void gfx_untrack_buf(struct gfx_tracker* tracker, GLuint buf);
void gfx_untrack_fbo(struct gfx_tracker* tracker, GLuint fbo);
void gfx_untrack_shader(struct gfx_tracker* tracker, GLuint shader);
#else
/* clang-format off */
#    define GFX_TRACKER(gfx) NULL
#    define gfx_track_tex(tracker, tex) do {} while (0)
#    define gfx_track_buf(tracker, buf) do {} while (0)
#    define gfx_track_fbo(tracker, fbo) do {} while (0)
#    define gfx_track_shader(tracker, shader) do {} while (0)

#    define gfx_untrack_tex(tracker, tex) do {} while (0)
#    define gfx_untrack_buf(tracker, buf) do {} while (0)
#    define gfx_untrack_fbo(tracker, fbo) do {} while (0)
#    define gfx_untrack_shader(tracker, shader) do {} while (0)
/* clang-format on */
#endif

struct gfx
{
    struct GLFWwindow* window;
    int                width, height;

#if defined(CLITHER_DEBUG_MEMORY)
    struct gfx_tracker tracker;
#endif

    FT_Library  ft_lib;
    struct font font;
    struct text text;

#if defined(CLITHER_GFX_DEBUG)
    struct debug debug;
#endif

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
};

struct aspect_ratio
{
    float scale_x, scale_y;
    float pad_x, pad_y;
};
