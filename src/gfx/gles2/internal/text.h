#pragma once

#include "clither/game/fpos.h"
#include "clither/game/q.h"
#include "clither/ui/ui.h"
#include "clither/util/strview.h"
#include "glad/gles2.h"

typedef struct FT_LibraryRec_* FT_Library;
typedef struct FT_FaceRec_*    FT_Face;

typedef struct hb_buffer_t hb_buffer_t;
typedef struct hb_font_t   hb_font_t;

struct aspect_ratio;
struct camera;
struct resource_shader;
struct resource_text;
struct gfx_text_hmap;

struct gfx_glyph_info
{
    int     start_x;
    int     start_y;
    int     end_x;
    int     end_y;
    GLfloat bearing_x;
    GLfloat bearing_y;
};

struct gfx_atlas
{
    struct text_glyph_hmap* glyph_hmap;

    uint8_t* data;
    int      width;
    int      height;

    /* Where to place the next glyph in the atlas */
    int next_x;
    int next_y;
    /* Tracks the largest glyph width in the current column of glyphs */
    int col_width;
};

struct gfx_font
{
    FT_Library ft_lib;
    FT_Face    ft_face;

    hb_font_t*   hb_font;
    hb_buffer_t* hb_buf;

    struct gfx_text_hmap*       text_hmap;
    struct gfx_atlas            atlas;
    struct text_vertex_buf_vec* vertices;

    GLuint program;
    GLuint tex_atlas;
    GLuint sAtlas;
    GLuint uAspectRatio;
    GLuint uPosCameraSpace;
    GLuint uSize;
    GLuint uColor;
};

int  gfx_gles2_font_init(struct gfx_font* font);
void gfx_gles2_font_deinit(struct gfx_font* font);
int  gfx_gles2_font_load(
     struct gfx_font*              font,
     const struct resource_text*   res,
     const struct resource_shader* shader);
void gfx_gles2_font_unload(struct gfx_font* font);

struct gfx_text
{
    struct fpos dimensions;
    GLuint      vbo;
    int         vertex_count;
    unsigned    was_used : 1;
};

void gfx_gles2_text_init(struct gfx_text* text);
void gfx_gles2_text_deinit(struct gfx_text* text);

struct fpos gfx_gles2_text_screen_size(
    struct gfx_font* font, struct strview str, GLfloat scale);

void gfx_gles2_text_prepare_draw(
    struct gfx_font* font, const struct aspect_ratio* ar);
void gfx_gles2_text_draw(
    struct strview       str,
    struct gfx_font*     font,
    struct qwpos         pos,
    struct fpos          screen_offset,
    GLfloat              scale,
    uint32_t             argb,
    enum ui_align        align,
    const struct camera* camera);
void gfx_gles2_text_draw_screen(
    struct strview   str,
    struct gfx_font* font,
    struct fpos      pos,
    GLfloat          scale,
    uint32_t         argb,
    enum ui_align    align);
void gfx_gles2_text_end_draw(void);

void gfx_gles2_text_clear_unused_from_cache(struct gfx_font* font);

#define gfx_gles2_text_shaped(text) ((text)->vertices != NULL)
