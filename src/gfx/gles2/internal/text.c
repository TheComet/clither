#include "clither/game/camera.h"
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
#include FT_ERRORS_H
#if defined(__clang__)
#    pragma clang diagnostic pop
#elif defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif

#include "clither/game/resource_pack.h"
#include "clither/util/hmap.h"
#include "clither/util/log.h"
#include "clither/util/str.h"
#include "hb-ft.h"
#include "hb.h"

/* Internal */
#include "./gfx.h"
#include "./shader.h"
#include "./text.h"

/* NOTE: 64 is the FreeType's internal pixel size multiplier */
#define PIXEL_FORMAT 64
#define TO_Q26_6(x)  ((x) * PIXEL_FORMAT)
#define TO_PIXELS(x) ((x) / PIXEL_FORMAT)

HMAP_DECLARE(static, text_glyph_hmap, uint32_t, struct gfx_glyph_info, 16)
HMAP_DEFINE(static, text_glyph_hmap, uint32_t, struct gfx_glyph_info, 16)

HMAP_DECLARE_STR(static, gfx_text_hmap, struct gfx_text, 16)
HMAP_DEFINE_STR(static, gfx_text_hmap, struct gfx_text, 16)

struct vertex
{
    GLfloat pos[2];
    GLfloat uv[2];
};
static const char* attr_bindings[] = {"vPosition", "vTexCoord", NULL};

static struct vertex vertex(GLfloat x, GLfloat y, GLfloat u, GLfloat v)
{
    struct vertex vertex;
    vertex.pos[0] = x;
    vertex.pos[1] = y;
    vertex.uv[0] = u;
    vertex.uv[1] = v;
    return vertex;
}

VEC_DECLARE(text_vertex_buf_vec, struct vertex, 16)
VEC_DEFINE(text_vertex_buf_vec, struct vertex, 16)

/* ------------------------------------------------------------------------- */
static int to_nearest_pow2(int value)
{
    int nearest = 2;
    while ((nearest <<= 1) < value)
        ;
    return nearest;
}

/* ------------------------------------------------------------------------- */
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

/* ------------------------------------------------------------------------- */
static void gfx_gles2_text_atlas_init(struct gfx_atlas* atlas)
{
    text_glyph_hmap_init(&atlas->glyph_hmap);
    atlas->data = NULL;
    atlas->width = 0;
    atlas->height = 0;
    atlas->next_x = 1;
    atlas->next_y = 1;
    atlas->col_width = 0;
}

/* ------------------------------------------------------------------------- */
static void gfx_gles2_text_atlas_deinit(struct gfx_atlas* atlas)
{
    if (atlas->data != NULL)
        mem_free(atlas->data);
    text_glyph_hmap_deinit(atlas->glyph_hmap);
}

/* ------------------------------------------------------------------------- */
static int add_glyph_to_atlas(
    struct gfx_glyph_info* info,
    struct gfx_font*       font,
    uint32_t               codepoint,
    uint8_t*               did_realloc)
{
    int          tex_width, tex_height;
    FT_GlyphSlot slot;
    int          y;
    const int    padding = 1;

    struct gfx_atlas* atlas = &font->atlas;

    FT_Error error = FT_Load_Glyph(font->ft_face, codepoint, FT_LOAD_DEFAULT);
    if (error)
    {
        log_warn(
            "Failed to load glyph with codepoint %u: %s\n",
            codepoint,
            FT_Error_String(error));
        return 0;
    }
    slot = font->ft_face->glyph;
    FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);

    if (atlas->col_width < (int)slot->bitmap.width)
        atlas->col_width = (int)slot->bitmap.width;

    tex_width = to_nearest_pow2(atlas->next_x + atlas->col_width);
    tex_height = to_nearest_pow2(atlas->next_y + slot->bitmap.rows);
    if (tex_height < atlas->height)
        tex_height = atlas->height;

    /* Begin new column if we reach upper Y limit of atlas size */
    if (tex_height > 1024)
    {
        atlas->next_x += atlas->col_width + padding;
        atlas->col_width = slot->bitmap.width;
        atlas->next_y = 0;
        tex_width = to_nearest_pow2(atlas->next_x + atlas->col_width);
        tex_height = atlas->height;
    }

    if (atlas->data != NULL && tex_width > atlas->width)
    {
        /* Adding a new column is more expensive because we have to copy over
         * all of the old glyphs to different memory offsets on the bigger
         * atlas */
        uint8_t* new_data = mem_alloc(tex_width * tex_height);
        if (new_data == NULL)
            return -1;
        memset(new_data, 0, tex_width * tex_height);
        for (y = 0; y < atlas->height; ++y)
            memcpy(
                new_data + y * tex_width,
                atlas->data + y * atlas->width,
                atlas->width);
        mem_free(atlas->data);
        atlas->data = new_data;
        *did_realloc = 1;
    }
    else if (atlas->data == NULL || tex_height > atlas->height)
    {
        /* Growing the atlas downwards preserves the offsets of the old glyphs,
         * so can just realloc(). Make sure to memset() the new space */
        uint8_t* new_data = mem_realloc(atlas->data, tex_width * tex_height);
        if (new_data == NULL)
            return -1;
        atlas->data = new_data;
        memset(
            atlas->data + atlas->width * atlas->height,
            0,
            (tex_width * tex_height) - (atlas->width * atlas->height));
        *did_realloc = 1;
    }
    atlas->width = tex_width;
    atlas->height = tex_height;

    /* Copy glyph bitmap into the atlas and advance positions */
    for (y = 0; y < (int)slot->bitmap.rows; ++y)
    {
        int y_dst = (y + atlas->next_y);
        memcpy(
            atlas->data + y_dst * atlas->width + atlas->next_x,
            slot->bitmap.buffer + y * slot->bitmap.width,
            slot->bitmap.width);
    }

    info->start_x = atlas->next_x;
    info->start_y = atlas->next_y;
    info->end_x = atlas->next_x + slot->bitmap.width;
    info->end_y = atlas->next_y + slot->bitmap.rows;
    info->bearing_x = TO_PIXELS((GLfloat)slot->metrics.horiBearingX);
    info->bearing_y = TO_PIXELS((GLfloat)slot->metrics.horiBearingY);
    atlas->next_y += slot->bitmap.rows + padding;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font->tex_atlas);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_ALPHA,
        atlas->width,
        atlas->height,
        0,
        GL_ALPHA,
        GL_UNSIGNED_BYTE,
        atlas->data);

    return 0;
}

/* ------------------------------------------------------------------------- */
static int add_glyphs_to_atlas(
    struct gfx_font* font,
    hb_glyph_info_t* hb_info,
    int              glyph_count,
    uint8_t*         did_realloc)
{
    int i;
    for (i = 0; i < glyph_count; ++i)
    {
        struct gfx_glyph_info* info;
        uint32_t               codepoint = hb_info[i].codepoint;
        switch (text_glyph_hmap_emplace_or_get(
            &font->atlas.glyph_hmap, codepoint, &info))
        {
            case HMAP_NEW:
                if (add_glyph_to_atlas(info, font, codepoint, did_realloc) != 0)
                    return -1;
                break;
            case HMAP_EXISTS: continue;
            case HMAP_OOM: return -1;
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
static int generate_mesh(
    struct gfx_text*     text,
    struct gfx_font*     font,
    hb_glyph_info_t*     hb_info,
    hb_glyph_position_t* hb_pos,
    int                  glyph_count)
{
    int               i;
    GLfloat           x, y;
    GLfloat           rescale;
    struct vertex*    vert;
    struct gfx_atlas* atlas = &font->atlas;

    text_vertex_buf_vec_clear(font->vertices);

    x = 0, y = 0;
    for (i = 0; i < glyph_count; ++i)
    {
        struct gfx_glyph_info* text_glyph_info =
            text_glyph_hmap_find(atlas->glyph_hmap, hb_info[i].codepoint);

        GLfloat x_offset = TO_PIXELS((GLfloat)hb_pos[i].x_offset);
        GLfloat y_offset = TO_PIXELS((GLfloat)hb_pos[i].y_offset);
        GLfloat width = text_glyph_info->end_x - text_glyph_info->start_x;
        GLfloat height = text_glyph_info->end_y - text_glyph_info->start_y;

        GLfloat x0 = x + x_offset + text_glyph_info->bearing_x;
        GLfloat y0 = floor(y + y_offset + text_glyph_info->bearing_y);
        GLfloat x1 = x0 + width;
        GLfloat y1 = y0 - height;

        GLfloat u0 = (GLfloat)text_glyph_info->start_x / (GLfloat)atlas->width;
        GLfloat v0 = (GLfloat)text_glyph_info->start_y / (GLfloat)atlas->height;
        GLfloat u1 = (GLfloat)text_glyph_info->end_x / (GLfloat)atlas->width;
        GLfloat v1 = (GLfloat)text_glyph_info->end_y / (GLfloat)atlas->height;

        x += TO_PIXELS((GLfloat)hb_pos[i].x_advance);
        y += TO_PIXELS((GLfloat)hb_pos[i].y_advance);

        text_vertex_buf_vec_push(&font->vertices, vertex(x0, y0, u0, v0));
        text_vertex_buf_vec_push(&font->vertices, vertex(x0, y1, u0, v1));
        text_vertex_buf_vec_push(&font->vertices, vertex(x1, y1, u1, v1));
        text_vertex_buf_vec_push(&font->vertices, vertex(x1, y0, u1, v0));
        text_vertex_buf_vec_push(&font->vertices, vertex(x0, y0, u0, v0));
        text_vertex_buf_vec_push(&font->vertices, vertex(x1, y1, u1, v1));
    }

    /* We want the height of "1" to mean the entire height of the screen */
    rescale = TO_PIXELS((GLfloat)font->ft_face->size->metrics.height);
    vec_for_each (font->vertices, vert)
    {
        vert->pos[0] = (vert->pos[0] * 2) / rescale * 2;
        vert->pos[1] = (vert->pos[1] * 2) / rescale * 2;
    }
    text->dimensions.x = x / rescale * 4;
    text->dimensions.y = 1.0;

    text->vertex_count = vec_count(font->vertices);
    glBindBuffer(GL_ARRAY_BUFFER, text->vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        text->vertex_count * sizeof(struct vertex),
        vec_data(font->vertices),
        GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return 0;
}

/* ------------------------------------------------------------------------- */
static int shape_text(
    struct gfx_font*      font,
    struct strview        str,
    hb_glyph_info_t**     hb_info,
    hb_glyph_position_t** hb_pos,
    int*                  glyph_count)
{
    unsigned int        glyph_count_u;
    const hb_tag_t      KernTag = HB_TAG('k', 'e', 'r', 'n');
    static hb_feature_t KerningOn = {KernTag, 1, 0, INT_MAX};

    hb_buffer_reset(font->hb_buf);
    hb_buffer_set_direction(font->hb_buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(font->hb_buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(
        font->hb_buf, hb_language_from_string("en", sizeof("en") - 1));
    hb_buffer_add_utf8(
        font->hb_buf, strview_data(str), strview_len(str), 0, strview_len(str));
    hb_shape(font->hb_font, font->hb_buf, &KerningOn, 1);

    *hb_info = hb_buffer_get_glyph_infos(font->hb_buf, &glyph_count_u);
    *hb_pos = hb_buffer_get_glyph_positions(font->hb_buf, &glyph_count_u);
    *glyph_count = (int)glyph_count_u;
    return 0;
}

/* ------------------------------------------------------------------------- */
static int regenerate_all_meshes(struct gfx_font* font)
{
    int16_t           idx;
    const struct str* str;
    struct gfx_text*  text;

    hmap_for_each (font->text_hmap, idx, str, text)
    {
        hb_glyph_info_t*     hb_info;
        hb_glyph_position_t* hb_pos;
        int                  count;

        (void)idx;
        if (shape_text(font, str_view(str), &hb_info, &hb_pos, &count) != 0)
            return -1;
        if (generate_mesh(text, font, hb_info, hb_pos, count) != 0)
            return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
static int
update_text(struct gfx_text* text, struct gfx_font* font, struct strview str)
{
    uint8_t              did_realloc;
    hb_glyph_info_t*     hb_info;
    hb_glyph_position_t* hb_pos;
    int                  glyph_count;

    if (shape_text(font, str, &hb_info, &hb_pos, &glyph_count) != 0)
        return -1;

    did_realloc = 0;
    if (add_glyphs_to_atlas(font, hb_info, glyph_count, &did_realloc) != 0)
        return -1;

    if (did_realloc)
        return regenerate_all_meshes(font);

    return generate_mesh(text, font, hb_info, hb_pos, glyph_count);
}

/* ------------------------------------------------------------------------- */
static struct gfx_text*
create_or_update_text(struct gfx_font* font, struct strview str)
{
    struct gfx_text* text = NULL;
    switch (gfx_text_hmap_emplace_or_get(&font->text_hmap, str, &text))
    {
        case HMAP_NEW:
            gfx_gles2_text_init(text);
            if (update_text(text, font, str) != 0)
                return NULL;
            /* fallthrough */
        case HMAP_EXISTS:
            text->was_used = 1;
            /* fallthrough */
        case HMAP_OOM: break;
    }

    return text;
}

/* ------------------------------------------------------------------------- */
int gfx_gles2_font_init(struct gfx_font* font)
{
    FT_Error ft_error = FT_Init_FreeType(&font->ft_lib);
    if (ft_error)
    {
        return log_err(
            "Failed to initialize FreeType library: %s\n",
            FT_Error_String(ft_error));
    }
    track_mem(font->ft_lib, 0);

    font->program = INVALID_HANDLE;
    font->tex_atlas = INVALID_UNIFORM_LOCATION;
    font->sAtlas = INVALID_UNIFORM_LOCATION;
    font->uAspectRatio = INVALID_UNIFORM_LOCATION;
    font->uPosCameraSpace = INVALID_UNIFORM_LOCATION;
    font->uSize = INVALID_UNIFORM_LOCATION;
    font->uColor = INVALID_UNIFORM_LOCATION;

    glGenTextures(1, &font->tex_atlas);
    gfx_track_tex(font->tex_atlas);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font->tex_atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    gfx_text_hmap_init(&font->text_hmap);
    gfx_gles2_text_atlas_init(&font->atlas);
    text_vertex_buf_vec_init(&font->vertices);

    return 0;
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_font_deinit(struct gfx_font* font)
{
    text_vertex_buf_vec_deinit(font->vertices);
    gfx_gles2_text_atlas_deinit(&font->atlas);
    gfx_text_hmap_deinit(font->text_hmap);

    gfx_untrack_tex(font->tex_atlas);
    glDeleteTextures(1, &font->tex_atlas);

    if (font->program != 0)
    {
        gfx_untrack_shader(font->program);
        glDeleteProgram(font->program);
    }

    untrack_mem(font->ft_lib);
    FT_Done_FreeType(font->ft_lib);
}

/* ------------------------------------------------------------------------- */
int gfx_gles2_font_load(
    struct gfx_font*              font,
    const struct resource_text*   res,
    const struct resource_shader* shader)
{
    int      ft_size;
    FT_Error error;

    error = FT_New_Face(font->ft_lib, str_cstr(res->font), 0, &font->ft_face);
    if (error)
    {
        log_err(
            "Failed to load font '%s': %s\n",
            str_cstr(res->font),
            FT_Error_String(error));
        goto ft_new_face_failed;
    }
    track_mem(font->ft_face, 0);
    force_ucs2_charmap(font->ft_face);

    /* NOTE: Don't use this if using FreeType's cache API */
    ft_size = TO_Q26_6(res->size);
    FT_Set_Char_Size(font->ft_face, ft_size, ft_size, res->dpi, res->dpi);

    font->hb_font = hb_ft_font_create(font->ft_face, NULL);
    if (font->hb_font == NULL)
    {
        log_err("Failed to create HarfBuzz font from FreeType face\n");
        goto hb_ft_font_create_failed;
    }
    track_mem(font->hb_font, 0);

    font->hb_buf = hb_buffer_create();
    if (font->hb_buf == NULL)
    {
        log_err("Failed to create HarfBuzz buffer\n");
        goto hb_buffer_create_failed;
    }
    track_mem(font->hb_buf, 0);

    CLITHER_DEBUG_ASSERT(font->program == 0);
    font->program = gfx_gles2_load_shader(shader->text, attr_bindings);
    if (font->program == 0)
        goto load_shader_failed;
    font->sAtlas =
        gfx_gles2_get_uniform_location_and_warn(font->program, "sAtlas");
    font->uAspectRatio =
        gfx_gles2_get_uniform_location_and_warn(font->program, "uAspectRatio");
    font->uPosCameraSpace = gfx_gles2_get_uniform_location_and_warn(
        font->program, "uPosCameraSpace");
    font->uSize =
        gfx_gles2_get_uniform_location_and_warn(font->program, "uSize");
    font->uColor =
        gfx_gles2_get_uniform_location_and_warn(font->program, "uColor");

    return 0;

load_shader_failed:
    untrack_mem(font->hb_buf);
    hb_buffer_destroy(font->hb_buf);
hb_buffer_create_failed:
    untrack_mem(font->hb_font);
    hb_font_destroy(font->hb_font);
hb_ft_font_create_failed:
    FT_Done_Face(font->ft_face);
ft_new_face_failed:
    return -1;
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_font_unload(struct gfx_font* font)
{
    int16_t          slot;
    struct str*      str;
    struct gfx_text* text;
    hmap_for_each (font->text_hmap, slot, str, text)
        (void)slot, (void)str, gfx_gles2_text_deinit(text);
    gfx_text_hmap_clear_compact(&font->text_hmap);

    if (font->program != 0)
    {
        gfx_untrack_shader(font->program);
        glDeleteProgram(font->program);
    }
    font->program = 0;

    untrack_mem(font->hb_buf);
    hb_buffer_destroy(font->hb_buf);

    untrack_mem(font->hb_font);
    hb_font_destroy(font->hb_font);

    untrack_mem(font->ft_face);
    FT_Done_Face(font->ft_face);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_text_init(struct gfx_text* text)
{
    glGenBuffers(1, &text->vbo);
    gfx_track_buf(text->vbo);

    text->dimensions = make_fpos(0, 0);
    text->was_used = 0;
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_text_deinit(struct gfx_text* text)
{
    gfx_untrack_buf(text->vbo);
    glDeleteBuffers(1, &text->vbo);
}

/* ------------------------------------------------------------------------- */
struct fpos gfx_gles2_text_screen_size(
    struct gfx_font* font, struct strview str, GLfloat scale)
{
    struct gfx_text* text = create_or_update_text(font, str);
    if (text != NULL)
        return make_fpos(
            text->dimensions.x * scale, text->dimensions.y * scale);
    return make_fpos(0, 0);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_text_prepare_draw(
    struct gfx_font* font, const struct aspect_ratio* ar)
{
    glUseProgram(font->program);
    glUniform2f(font->uAspectRatio, ar->scale_x, ar->scale_y);
    glUniform1i(font->sAtlas, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font->tex_atlas);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_text_end_draw(void)
{
    glUseProgram(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_text_draw(
    struct strview       str,
    struct gfx_font*     font,
    struct qwpos         pos,
    struct fpos          screen_offset,
    uint32_t             argb,
    float                scale,
    enum ui_align        align,
    const struct camera* camera)
{
    struct
    {
        GLfloat x, y;
    } pos_cameraSpace;

    struct gfx_text* text = create_or_update_text(font, str);
    if (text == NULL)
        return;

    pos_cameraSpace.x = qw_to_float(pos.x) - qw_to_float(camera->pos.x);
    pos_cameraSpace.y = qw_to_float(pos.y) - qw_to_float(camera->pos.y);
    pos_cameraSpace.y *= qw_to_float(camera->scale);
    pos_cameraSpace.x *= qw_to_float(camera->scale);
    pos_cameraSpace.x += screen_offset.x;
    pos_cameraSpace.y += screen_offset.y;

    switch (align)
    {
        case UI_ALIGN_LEFT: break;
        case UI_ALIGN_CENTER:
            pos_cameraSpace.x -= text->dimensions.x * scale / 2.0f;
            break;
        case UI_ALIGN_RIGHT:
            pos_cameraSpace.x -= text->dimensions.x * scale;
            break;
    }

    glUniform1f(font->uSize, scale);
    glUniform2f(font->uPosCameraSpace, pos_cameraSpace.x, pos_cameraSpace.y);
    glUniform4f(
        font->uColor,
        ((argb >> 16) & 0xFF) / 255.0,
        ((argb >> 8) & 0xFF) / 255.0,
        ((argb >> 0) & 0xFF) / 255.0,
        ((argb >> 24) & 0xFF) / 255.0);

    glBindBuffer(GL_ARRAY_BUFFER, text->vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(struct vertex),
        (void*)offsetof(struct vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(struct vertex),
        (void*)offsetof(struct vertex, uv));
    glDrawArrays(GL_TRIANGLES, 0, text->vertex_count);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_text_draw_screen(
    struct strview   str,
    struct gfx_font* font,
    struct fpos      pos,
    uint32_t         argb,
    GLfloat          scale,
    enum ui_align    align)
{
    struct gfx_text* text = create_or_update_text(font, str);
    if (text == NULL)
        return;

    switch (align)
    {
        case UI_ALIGN_LEFT: break;
        case UI_ALIGN_CENTER: pos.x -= text->dimensions.x * scale / 2.0f; break;
        case UI_ALIGN_RIGHT: pos.x -= text->dimensions.x * scale; break;
    }

    glUniform1f(font->uSize, scale);
    glUniform2f(font->uPosCameraSpace, pos.x, pos.y);
    glUniform4f(
        font->uColor,
        ((argb >> 16) & 0xFF) / 255.0,
        ((argb >> 8) & 0xFF) / 255.0,
        ((argb >> 0) & 0xFF) / 255.0,
        ((argb >> 24) & 0xFF) / 255.0);

    glBindBuffer(GL_ARRAY_BUFFER, text->vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(struct vertex),
        (void*)offsetof(struct vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(struct vertex),
        (void*)offsetof(struct vertex, uv));
    glDrawArrays(GL_TRIANGLES, 0, text->vertex_count);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_text_clear_unused_from_cache(struct gfx_font* font)
{
    int16_t           idx;
    const struct str* str;
    struct gfx_text*  text;

    hmap_for_each (font->text_hmap, idx, str, text)
        if (text->was_used == 0)
        {
            (void)str;
            gfx_gles2_text_deinit(text);
            gfx_text_hmap_erase_slot(font->text_hmap, idx);
        }

    hmap_for_each (font->text_hmap, idx, str, text)
        (void)idx, (void)str, text->was_used = 0;
}
