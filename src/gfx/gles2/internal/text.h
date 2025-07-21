#pragma once

#include "glad/gles2.h"

typedef struct FT_LibraryRec_* FT_Library;
typedef struct FT_FaceRec_*    FT_Face;

typedef struct hb_buffer_t hb_buffer_t;
typedef struct hb_font_t   hb_font_t;

struct resource_pack;
struct resource_text;

struct font
{
    FT_Library ft_lib;
    FT_Face    ft_face;

    hb_font_t*   hb_font;
    hb_buffer_t* hb_buf;
};

int  gfx_gles2_font_init(struct font* font);
void gfx_gles2_font_deinit(struct font* font);
int  gfx_gles2_font_load(struct font* font, const struct resource_text* res);
void gfx_gles2_font_unload(struct font* font);

struct text_mat
{
    GLuint program;
};

int  gfx_gles2_text_mat_init(struct text_mat* mat);
void gfx_gles2_text_mat_deinit(struct text_mat* mat);
int  gfx_gles2_text_mat_load(
     struct text_mat* mat, const struct resource_pack* pack);
void gfx_gles2_text_mat_unload(struct text_mat* mat);

struct text
{
    GLuint vbo;
    GLuint tex;
};

int  gfx_gles2_text_init(struct text* text);
void gfx_gles2_text_deinit(struct text* text);
void gfx_gles2_text_shape(
    struct font* font, struct text* text, const char* str);
