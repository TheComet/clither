#include "clither/util/tracker.h"
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

#include "clither/game/resource_pack.h"
#include "clither/util/log.h"
#include "clither/util/str.h"
#include "hb-ft.h"
#include "hb.h"

/* Internal */
#include "./gfx.h"
#include "./shader.h"
#include "./text.h"

static int force_ucs2_charmap(FT_Face face)
{
    int i;
    for (i = 0; i < face->num_charmaps; i++)
    {
        if (((face->charmaps[i]->platform_id == 0) &&
             (face->charmaps[i]->encoding_id == 3)) ||
            ((face->charmaps[i]->platform_id == 3) &&
             (face->charmaps[i]->encoding_id == 1)))
        {
            return FT_Set_Charmap(face, face->charmaps[i]);
        }
    }
    return -1;
}

int gfx_gles2_font_init(struct font* font)
{
    FT_Error ft_error = FT_Init_FreeType(&font->ft_lib);
    if (ft_error)
        return log_err(
            "Failed to initialize FreeType library: %s\n",
            FT_Error_String(ft_error));
    track_mem(font->ft_lib, 0);

    return 0;
}

void gfx_gles2_font_deinit(struct font* font)
{
    untrack_mem(font->ft_lib);
    FT_Done_FreeType(font->ft_lib);
}

int gfx_gles2_font_load(struct font* font, const struct resource_text* res)
{
    int      ft_size;
    FT_Error error =
        FT_New_Face(font->ft_lib, str_cstr(res->font_file), 0, &font->ft_face);
    if (error)
    {
        return log_err(
            "Failed to load font '%s': %s\n",
            str_cstr(res->font_file),
            FT_Error_String(error));
    }
    track_mem(font->ft_face, 0);
    force_ucs2_charmap(font->ft_face);

    /* NOTE: Don't use this if using FreeType's cache API */
    /* NOTE: 64 is the FreeType's internal pixel size multiplier */
    ft_size = res->size * 64;
    FT_Set_Char_Size(
        font->ft_face, ft_size, ft_size, res->device_hdpi, res->device_vdpi);

    font->hb_font = hb_ft_font_create(font->ft_face, NULL);
    font->hb_buf = hb_buffer_create();
    track_mem(font->hb_font, 0);
    track_mem(font->hb_buf, 0);

    return 0;
}

void gfx_gles2_font_unload(struct font* font)
{
    untrack_mem(font->hb_buf);
    untrack_mem(font->hb_font);
    untrack_mem(font->ft_face);

    hb_buffer_destroy(font->hb_buf);
    hb_font_destroy(font->hb_font);
    FT_Done_Face(font->ft_face);
}

int gfx_gles2_text_mat_init(struct text_mat* mat)
{
    mat->program = 0;
    return 0;
}

void gfx_gles2_text_mat_deinit(struct text_mat* mat)
{
    if (mat->program != 0)
    {
        gfx_untrack_shader(mat->program);
        glDeleteProgram(mat->program);
    }
}

static const char* attr_bindings[2] = {"vPosition", NULL};

int gfx_gles2_text_mat_load(
    struct text_mat* mat, const struct resource_pack* pack)
{
    CLITHER_DEBUG_ASSERT(mat->program == 0);
    mat->program =
        gfx_gles2_load_shader(pack->shaders.glsl.text, attr_bindings);
    if (mat->program == 0)
        return -1;
    gfx_track_shader(mat->program);

    return 0;
}

void gfx_gles2_text_mat_unload(struct text_mat* mat)
{
    if (mat->program != 0)
    {
        gfx_untrack_shader(mat->program);
        glDeleteProgram(mat->program);
    }
    mat->program = 0;
}

int gfx_gles2_text_init(struct text* text)
{
    glGenBuffers(1, &text->vbo);
    gfx_track_buf(text->vbo);

    glGenTextures(1, &text->tex);
    gfx_track_tex(text->tex);
    glBindTexture(GL_TEXTURE_2D, text->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    return 0;
}

void gfx_gles2_text_deinit(struct text* text)
{
    gfx_untrack_tex(text->tex);
    glDeleteTextures(1, &text->tex);

    gfx_untrack_buf(text->vbo);
    glDeleteBuffers(1, &text->vbo);
}

void gfx_gles2_text_shape(struct font* font, struct text* text, const char* str)
{
    hb_buffer_reset(font->hb_buf);

    hb_buffer_set_direction(font->hb_buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(font->hb_buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(
        font->hb_buf, hb_language_from_string("en", sizeof("en") - 1));
    size_t length = strlen(str);

    hb_buffer_add_utf8(font->hb_buf, str, length, 0, length);

    const hb_tag_t      KernTag = HB_TAG('k', 'e', 'r', 'n');
    static hb_feature_t KerningOn = {KernTag, 1, 0, INT_MAX};

    hb_shape(font->hb_font, font->hb_buf, &KerningOn, 1);

    unsigned int     glyphCount;
    hb_glyph_info_t* glyphInfo =
        hb_buffer_get_glyph_infos(font->hb_buf, &glyphCount);
    hb_glyph_position_t* glyphPos =
        hb_buffer_get_glyph_positions(font->hb_buf, &glyphCount);

    int i;
    for (i = 0; i < (int)glyphCount; ++i)
    {
        FT_Load_Glyph(font->ft_face, glyphInfo[i].codepoint, FT_LOAD_DEFAULT);

        FT_GlyphSlot slot = font->ft_face->glyph;
        FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);

        unsigned char* glyph_buffer = slot->bitmap.buffer;
        int            glyph_width = slot->bitmap.width;
        int            glyph_height = slot->bitmap.rows;
        float          glyph_bearing_x = slot->bitmap_left;
        float          glyph_bearing_y = slot->bitmap_top;

        int twidth = pow(2, ceil(log(glyph_width) / log(2)));
        int theight = pow(2, ceil(log(glyph_height) / log(2)));

        // auto tdata = new unsigned char[twidth * theight]();

        // for (int iy = 0; iy < glyph->height; ++iy)
        //{
        //     memcpy(
        //         tdata + iy * twidth,
        //         glyph->buffer + iy * glyph->width,
        //         glyph->width);
        // }

        // float s0 = 0.0;
        // float t0 = 0.0;
        // float s1 = (float)glyph->width / twidth;
        // float t1 = (float)glyph->height / theight;
        // float xa = (float)glyphPos[i].x_advance / 64;
        // float ya = (float)glyphPos[i].y_advance / 64;
        // float xo = (float)glyphPos[i].x_offset / 64;
        // float yo = (float)glyphPos[i].y_offset / 64;
        // float x0 = x + xo + glyph->bearing_x;
        // float y0 = floor(y + yo + glyph->bearing_y);
        // float x1 = x0 + glyph->width;
        // float y1 = floor(y0 - glyph->height);

        // gl::Vertex* vertices = new gl::Vertex[4];
        // vertices[0] = gl::Vertex(x0, y0, s0, t0);
        // vertices[1] = gl::Vertex(x0, y1, s0, t1);
        // vertices[2] = gl::Vertex(x1, y1, s1, t1);
        // vertices[3] = gl::Vertex(x1, y0, s1, t0);

        // unsigned short* indices = new unsigned short[6];
        // indices[0] = 0;
        // indices[1] = 1;
        // indices[2] = 2;
        // indices[3] = 0;
        // indices[4] = 2;
        // indices[5] = 3;

        // gl::Mesh* m = new gl::Mesh;

        // m->indices = indices;
        // m->textureData = tdata;

        //// don't do this!! use atlas texture instead
        // m->textureId = gl::getTextureId(twidth, theight);

        // m->vertices = vertices;
        // m->nbIndices = 6;
        // m->nbVertices = 4;

        // gl::uploadTextureData(m->textureId, twidth, theight, tdata);

        // meshes.push_back(m);

        // x += xa;
        // y += ya;
    }

    // glBindBuffer(GL_ARRAY_BUFFER, text->vbo);
    // glBufferData(...);
    // glBindBuffer(GL_ARRAY_BUFFER, 0);
}
