#pragma once

#include "clither/game/q.h"
#include "glad/gles2.h"

typedef struct FT_LibraryRec_* FT_Library;
typedef struct FT_FaceRec_*    FT_Face;

typedef struct hb_buffer_t hb_buffer_t;
typedef struct hb_font_t   hb_font_t;

struct aspect_ratio;
struct camera;
struct resource_shader;
struct resource_text;

struct text_glyph_info
{
    int     start_x;
    int     start_y;
    int     end_x;
    int     end_y;
    GLfloat bearing_x;
    GLfloat bearing_y;
};

struct text_atlas
{
    struct text_glyph_hmap* glyphs;

    uint8_t* data;
    int      width;
    int      height;

    /* Where to place the next glyph in the atlas */
    int next_x;
    int next_y;
    /* Tracks the largest glyph width in the current column of glyphs */
    int row_width;
};

struct font
{
    FT_Library ft_lib;
    FT_Face    ft_face;

    hb_font_t*   hb_font;
    hb_buffer_t* hb_buf;

    struct text_atlas atlas;

    GLuint program;
    GLuint tex_atlas;
    GLuint sAtlas;
    GLuint uAspectRatio;
    GLuint uPosCameraSpace;
    GLuint uSize;
};

int  gfx_gles2_font_init(struct font* font);
void gfx_gles2_font_deinit(struct font* font);
int  gfx_gles2_font_load(
     struct font*                  font,
     const struct resource_text*   res,
     const struct resource_shader* shader);
void gfx_gles2_font_unload(struct font* font);

struct text
{
    struct text_vertex_buf_vec* vertices;
    GLuint                      vbo;
    unsigned                    was_used : 1;
};

void gfx_gles2_text_init(struct text* text);
void gfx_gles2_text_deinit(struct text* text);
void gfx_gles2_text_shape(
    struct text* text, struct font* font, const char* str);

void gfx_gles2_text_prepare_draw(
    const struct font* font, const struct aspect_ratio* ar);
void gfx_gles2_text_draw(
    const struct text*   text,
    const struct font*   font,
    struct qwpos         pos,
    float                screen_off_x,
    float                screen_off_y,
    float                scale,
    const struct camera* camera);
void gfx_gles2_text_end_draw(void);

#define gfx_gles2_text_shaped(text) ((text)->vertices != NULL)
