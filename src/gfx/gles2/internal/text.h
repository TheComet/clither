#pragma once

#include "glad/gles2.h"

/* Font rendering */
#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wlong-long"
#elif defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wlong-long"
#endif

#include "ft2build.h"
#include FT_BITMAP_H

#if defined(__clang__)
#    pragma clang diagnostic pop
#elif defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif

//#include "hb.h"

struct font
{
    FT_Face ft_face;

    //hb_font_t*   hb_font;
    //hb_buffer_t* hb_buf;

    GLuint texAtlas;
};

struct text
{
    GLuint vbo;
};
