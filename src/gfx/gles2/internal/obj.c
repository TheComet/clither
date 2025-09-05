#include "./gfx.h"
#include "./obj.h"
#include "./shader.h"
#include "clither/game/resource_pack.h"
#include "clither/platform/mfile.h"
#include "clither/util/hmap.h"
#include <ctype.h>

VEC_DEFINE(gfx_obj_submesh_vec, struct gfx_obj_submesh, 8)
VEC_DEFINE(gfx_obj_vertex_vec, struct gfx_obj_vertex, 32)

VEC_DECLARE(float_vec, float, 32)
VEC_DEFINE(float_vec, float, 32)
VEC_DECLARE(index_buffer_vec, GLuint, 32)
VEC_DEFINE(index_buffer_vec, GLuint, 32)

static const char vs[] =
    "precision mediump float;\n"
    "attribute vec3 vPosition;\n"
    "attribute vec2 vTexCoord;\n"
    "uniform mat4 sMvp;\n"
    "varying vec2 fTexCoord;\n"
    "void main()\n"
    "{\n"
    "    fTexCoord = vTexCoord;\n"
    "    vec4 pos = vec4(vPosition * 0.5, 1.0) * sMvp;\n"
    "    gl_Position = pos;\n"
    "}\n";
static const char fs[] =
    "precision mediump float;\n"
    "varying vec2 fTexCoord;\n"
    "void main()\n"
    "{\n"
    "    gl_FragColor = vec4(fTexCoord.xy, 0.0, 1.0);\n"
    "}\n";

struct index_hmap_kvs
{
    struct gfx_obj_vertex* keys;
    int*                   values;
};

HMAP_DECLARE_FULL(
    static,
    index_hmap,
    hash32,
    struct gfx_obj_vertex*,
    int,
    32,
    struct index_hmap_kvs)

static hash32 index_hmap_hash(struct gfx_obj_vertex* key)
{
    return hash32_jenkins_oaat(key, sizeof(*key));
}

static int index_hmap_kvs_alloc(
    struct index_hmap_kvs* kvs,
    struct index_hmap_kvs* old_kvs,
    int32_t                capacity)
{
    (void)old_kvs;
    if ((kvs->keys = mem_alloc(sizeof(*kvs->keys) * capacity)) == NULL)
        return -1;
    if ((kvs->values = mem_alloc(sizeof(*kvs->values) * capacity)) == NULL)
    {
        mem_free(kvs->keys);
        return -1;
    }
    return 0;
}
static void index_hmap_kvs_free(struct index_hmap_kvs* kvs, int32_t capacity)
{
    (void)capacity;
    mem_free(kvs->values);
    mem_free(kvs->keys);
}
static void
index_hmap_kvs_free_old(struct index_hmap_kvs* kvs, int32_t capacity)
{
    (void)capacity;
    index_hmap_kvs_free(kvs, capacity);
}
static struct gfx_obj_vertex*
index_hmap_kvs_get_key(const struct index_hmap_kvs* kvs, int32_t slot)
{
    return &kvs->keys[slot];
}
static void index_hmap_kvs_set_key(
    struct index_hmap_kvs* kvs, int32_t slot, struct gfx_obj_vertex* key)
{
    kvs->keys[slot] = *key;
}
static int
index_hmap_kvs_keys_equal(struct gfx_obj_vertex* k1, struct gfx_obj_vertex* k2)
{
    return k1->pos[0] == k2->pos[0] && k1->pos[1] == k2->pos[1] &&
           k1->pos[2] == k2->pos[2] && k1->uv[0] == k2->uv[0] &&
           k1->uv[1] == k2->uv[1];
}
static int*
index_hmap_kvs_get_value(const struct index_hmap_kvs* kvs, int32_t slot)
{
    return &kvs->values[slot];
}
static void index_hmap_kvs_set_value(
    struct index_hmap_kvs* kvs, int32_t slot, const int* value)
{
    kvs->values[slot] = *value;
}

HMAP_DEFINE_FULL(
    static,
    index_hmap,
    hash32,
    struct gfx_obj_vertex*,
    int,
    32,
    index_hmap_hash,
    index_hmap_kvs_alloc,
    index_hmap_kvs_free_old,
    index_hmap_kvs_free,
    index_hmap_kvs_get_key,
    index_hmap_kvs_set_key,
    index_hmap_kvs_keys_equal,
    index_hmap_kvs_get_value,
    index_hmap_kvs_set_value,
    128,
    70)

enum token
{
    TOK_ERROR = -1,
    TOK_END = 0,
    TOK_SLASH = '/',
    TOK_IDENT = 256,
    TOK_FLOAT,
    TOK_INTEGER
};

struct parser
{
    union
    {
        struct strview str;
        int            int_literal;
        float          float_literal;
    } value;
    const char* filename;
    const char* data;
    int         tail, head, end;
};

/* ------------------------------------------------------------------------- */
static const char* emph_style(void)
{
    return "\033[1;37m";
}
static const char* underline_style(void)
{
    return "\033[1;31m";
}
static const char* error_style(void)
{
    return "\033[1;31m";
}
static const char* reset_style(void)
{
    return "\033[0m";
}

/* ------------------------------------------------------------------------- */
static void print_vflc(
    const char*    filename,
    const char*    source,
    struct strview loc,
    const char*    fmt,
    va_list        ap)
{
    int i;
    int l1, c1;

    l1 = 1, c1 = 1;
    for (i = 0; i != loc.off; i++)
    {
        c1++;
        if (source[i] == '\n')
            l1++, c1 = 1;
    }

    fprintf(
        stderr,
        "%s%s:%d:%d:%s ",
        emph_style(),
        filename,
        l1,
        c1,
        reset_style());
    fprintf(stderr, "%s[obj] error:%s ", error_style(), reset_style());
    vfprintf(stderr, fmt, ap);
}

/* ------------------------------------------------------------------------- */
static void print_excerpt(const char* source, struct strview loc)
{
    int            i;
    int            l1, c1, l2, c2;
    int            indent, max_indent;
    int            gutter_indent;
    int            line;
    struct strview block;

    /* Calculate line column as well as beginning of block. The goal is to make
     * "block" point to the first character in the line that contains the
     * location. */
    l1 = 1, c1 = 1, block.off = 0;
    for (i = 0; i != loc.off; i++)
    {
        c1++;
        if (source[i] == '\n')
            l1++, c1 = 1, block.off = i + 1;
    }

    /* Calculate line/column of where the location ends */
    l2 = l1, c2 = c1;
    for (i = 0; i != loc.len; i++)
    {
        c2++;
        if (source[loc.off + i] == '\n')
            l2++, c2 = 1;
    }

    /* Find the end of the line for block */
    block.len = loc.off - block.off + loc.len;
    for (; source[loc.off + i]; block.len++, i++)
        if (source[loc.off + i] == '\n')
            break;

    /* We also keep track of the minimum indentation. This is used to unindent
     * the block of code as much as possible when printing out the excerpt. */
    max_indent = 10000;
    for (i = 0; i != block.len;)
    {
        indent = 0;
        for (; i != block.len; ++i, ++indent)
        {
            if (source[block.off + i] != ' ' && source[block.off + i] != '\t')
                break;
        }

        if (max_indent > indent)
            max_indent = indent;

        while (i != block.len)
            if (source[block.off + i++] == '\n')
                break;
    }

    /* Unindent columns */
    c1 -= max_indent;
    c2 -= max_indent;

    /* Find width of the largest line number. This sets the indentation of the
     * gutter */
    gutter_indent = snprintf(NULL, 0, "%d", l2);
    gutter_indent += 2; /* Padding on either side of the line number */

    /* Print line number, gutter, and block of code */
    line = l1;
    for (i = 0; i != block.len;)
    {
        fprintf(stderr, "%*d | ", gutter_indent - 1, line);

        if (i >= loc.off - block.off && i <= loc.off - block.off + loc.len)
            fprintf(stderr, "%s", underline_style());

        indent = 0;
        while (i != block.len)
        {
            if (i == loc.off - block.off)
                fprintf(stderr, "%s", underline_style());
            if (i == loc.off - block.off + loc.len)
                fprintf(stderr, "%s", reset_style());

            if (indent++ >= max_indent)
                putc(source[block.off + i], stderr);

            if (source[block.off + i++] == '\n')
            {
                if (i >= loc.off - block.off &&
                    i <= loc.off - block.off + loc.len)
                    fprintf(stderr, "%s", reset_style());
                break;
            }
        }
        line++;
    }
    fprintf(stderr, "%s\n", reset_style());

    /* print underline */
    if (c2 > c1)
    {
        fprintf(stderr, "%*s|%*s", gutter_indent, "", c1, "");
        fprintf(stderr, "%s", underline_style());
        putc('^', stderr);
        for (i = c1 + 1; i < c2; ++i)
            putc('~', stderr);
        fprintf(stderr, "%s", reset_style());
    }
    else
    {
        int col, max_col;

        fprintf(stderr, "%*s| ", gutter_indent, "");
        fprintf(stderr, "%s", underline_style());
        for (i = 1; i < c2; ++i)
            putc('~', stderr);
        for (; i < c1; ++i)
            putc(' ', stderr);
        putc('^', stderr);

        /* Have to find length of the longest line */
        col = 1, max_col = 1;
        for (i = 0; i != block.len; ++i)
        {
            if (max_col < col)
                max_col = col;
            col++;
            if (source[block.off + i] == '\n')
                col = 1;
        }
        max_col -= max_indent;

        for (i = c1 + 1; i < max_col; ++i)
            putc('~', stderr);
        fprintf(stderr, "%s", reset_style());
    }

    putc('\n', stderr);
}

/* ------------------------------------------------------------------------- */
static void
parser_init(struct parser* p, const char* filename, const void* data, int size)
{
    p->filename = filename;
    p->data = (const char*)data;
    p->end = size;
    p->head = p->tail = 0;
}

/* ------------------------------------------------------------------------- */
static int parser_error(const struct parser* p, const char* fmt, ...)
{
    va_list        ap;
    struct strview loc;

    loc.off = p->tail;
    loc.len = p->head - p->tail;

    va_start(ap, fmt);
    print_vflc(p->filename, p->data, loc, fmt, ap);
    va_end(ap);
    print_excerpt(p->data, loc);

    return -1;
}

/* ------------------------------------------------------------------------- */
static enum token scan_next(struct parser* p)
{
    p->tail = p->head;
    while (p->head != p->end)
    {
        /* Skip comments */
        if (p->data[p->head] == '#')
        {
            for (p->head++; p->head != p->end; p->head++)
                if (p->data[p->head] == '\n')
                {
                    p->head++;
                    break;
                }
            p->tail = p->head;
            continue;
        }

        if (p->data[p->head] == '/')
            return p->data[p->head++];

        /* Number */
        if (isdigit(p->data[p->head]) || p->data[p->head] == '-')
        {
            char is_neg = p->data[p->head] == '-';
            if (p->data[p->head] == '-')
                p->head++;

            p->value.int_literal = 0;
            for (; p->head != p->end && isdigit(p->data[p->head]); ++p->head)
            {
                p->value.int_literal *= 10;
                p->value.int_literal += p->data[p->head] - '0';
            }
            /* It is actually a float */
            if (p->head != p->end && p->data[p->head] == '.')
            {
                float fraction = 1.0;
                p->value.float_literal = (float)p->value.int_literal;
                for (p->head++; p->head != p->end && isdigit(p->data[p->head]);
                     ++p->head)
                {
                    fraction *= 0.1;
                    p->value.float_literal +=
                        fraction * (float)(p->data[p->head] - '0');
                }
                if (p->head != p->end && p->data[p->head] == 'f')
                    ++p->head;

                if (is_neg)
                    p->value.float_literal = -p->value.float_literal;
                return TOK_FLOAT;
            }

            if (is_neg)
                p->value.int_literal = -p->value.int_literal;
            return TOK_INTEGER;
        }

        /* Tag/Identifier [a-zA-Z_-][a-zA-Z0-9_-]* */
        if (isalpha(p->data[p->head]))
        {
            p->value.str.data = p->data;
            p->value.str.off = p->head++;
            while (p->head != p->end &&
                   (isalnum(p->data[p->head]) || p->data[p->head] == '_' ||
                    p->data[p->head] == '.' || p->data[p->head] == '/'))
            {
                p->head++;
            }
            p->value.str.len = p->head - p->value.str.off;
            return TOK_IDENT;
        }

        p->tail = ++p->head;
    }

    return TOK_END;
}

/* ------------------------------------------------------------------------- */
static int parse_mtl(struct parser* p)
{
    while (1)
    {
        enum token tok = scan_next(p);
        if (tok == TOK_END || tok == TOK_ERROR)
            return tok;

        if (tok != TOK_IDENT)
            return parser_error(p, "Unexpected token.\n");

        if (strview_eq_cstr(p->value.str, "newmtl"))
        {
            if (scan_next(p) != TOK_IDENT)
                return parser_error(p, "Expected a material name.\n");
        }
        else if (strview_eq_cstr(p->value.str, "Kd"))
        {
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Expected RGB values for diffuse color.\n");
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Expected RGB values for diffuse color.\n");
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Expected RGB values for diffuse color.\n");
        }
        else if (strview_eq_cstr(p->value.str, "Ka"))
        {
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Expected RGB values for ambient color.\n");
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Expected RGB values for ambient color.\n");
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Expected RGB values for ambient color.\n");
        }
        else if (strview_eq_cstr(p->value.str, "Ks"))
        {
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Expected RGB values for specular color.\n");
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Expected RGB values for specular color.\n");
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Expected RGB values for specular color.\n");
        }
        else if (strview_eq_cstr(p->value.str, "Ns"))
        {
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Expected float for specular exponent.\n");
        }
        else if (strview_eq_cstr(p->value.str, "Ke"))
        {
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Expected RGB values for emissive color.\n");
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Expected RGB values for emissive color.\n");
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Expected RGB values for emissive color.\n");
        }
        else if (strview_eq_cstr(p->value.str, "Ni"))
        {
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(p, "Expected float for optical density.\n");
        }
        else if (strview_eq_cstr(p->value.str, "d"))
        {
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(p, "Expected float for dissolve (1-Tr).\n");
        }
        else if (strview_eq_cstr(p->value.str, "Tr"))
        {
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(p, "Expected float for transparency.\n");
        }
        else if (strview_eq_cstr(p->value.str, "illum"))
        {
            if (scan_next(p) != TOK_INTEGER)
                return parser_error(
                    p, "Expected index (1-10) for illumination model.\n");
            /*
             * 0. Color on and Ambient off
             * 1. Color on and Ambient on
             * 2. Highlight on
             * 3. Reflection on and Ray trace on
             * 4. Transparency: Glass on, Reflection: Ray trace on
             * 5. Reflection: Fresnel on and Ray trace on
             * 6. Transparency: Refraction on, Reflection: Fresnel off and Ray
             * trace on
             * 7. Transparency: Refraction on, Reflection: Fresnel on and Ray
             * trace on
             * 8. Reflection on and Ray trace off
             * 9. Transparency: Glass on, Reflection: Ray trace off
             * 10. Casts shadows onto invisible surfaces
             */
        }
        else if (strview_eq_cstr(p->value.str, "map_Kd"))
        {
            if (scan_next(p) != TOK_IDENT)
                return parser_error(
                    p, "Expected relative file name of a texture.\n");
        }
        else
        {
            return parser_error(p, "Unknown tag\n");
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
static int parse_mtl_file(const char* obj_filepath, struct strview mtl_filename)
{
    struct mfile  mf;
    struct parser p;
    struct str*   filepath;

    str_init(&filepath);

    if (str_set_cstr(&filepath, obj_filepath) != 0)
        goto set_filepath_failed;
    str_dirname(filepath);
    if (str_join_path(&filepath, mtl_filename) != 0)
        goto set_filepath_failed;

    log_dbg("Loading mtl %s\n", str_cstr(filepath));
    if (mfile_map_read(&mf, str_cstr(filepath), 1) != 0)
        goto open_mtl_failed;

    parser_init(&p, str_cstr(filepath), mf.address, mf.size);
    if (parse_mtl(&p) != 0)
        goto parse_mtl_failed;

    mfile_unmap(&mf);
    str_deinit(filepath);
    return 0;

parse_mtl_failed:
    mfile_unmap(&mf);
open_mtl_failed:
set_filepath_failed:
    str_deinit(filepath);
    return -1;
}

/* ------------------------------------------------------------------------- */
static int create_submesh(
    struct gfx_obj_submesh_vec** submeshes,
    struct index_buffer_vec*     index_buffer)
{
    struct gfx_obj_submesh* mesh;
    static const char*      attr_bindings[] = {"vPosition", "vTexCoord", NULL};

    if (vec_count(index_buffer) == 0)
        return 0;

    mesh = gfx_obj_submesh_vec_emplace(submeshes);
    if (mesh == NULL)
        return -1;

    mesh->program = gfx_gles2_load_shader_str(vs, fs, attr_bindings);
    if (mesh->program == INVALID_HANDLE)
        return -1;
    mesh->sMvp = gfx_gles2_get_uniform_location_and_warn(mesh->program, "sMvp");

    mesh->index_count = vec_count(index_buffer);
    glGenBuffers(1, &mesh->ibo);
    gfx_track_buf(mesh->ibo, "obj.c::create_submesh()");
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        vec_count(index_buffer) * sizeof(GLuint),
        vec_data(index_buffer),
        GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    index_buffer_vec_clear(index_buffer);

    return 0;
}

/* ------------------------------------------------------------------------- */
static int parse_obj(
    struct parser*              p,
    struct gfx_obj*             obj,
    struct float_vec**          obj_v,
    struct float_vec**          obj_n,
    struct float_vec**          obj_uv,
    struct gfx_obj_vertex_vec** vertex_buffer,
    struct index_buffer_vec**   index_buffer,
    struct index_hmap**         index_hmap)
{
    while (1)
    {
        enum token tok = scan_next(p);
    reswitch_tok:
        if (tok == TOK_END)
            break;
        if (tok == TOK_ERROR)
            return -1;

        if (tok != TOK_IDENT)
            return parser_error(p, "Unexpected token.\n");

        if (strview_eq_cstr(p->value.str, "mtllib"))
        {
            if (scan_next(p) != TOK_IDENT)
                return parser_error(p, "Expected a file name.\n");
            if (parse_mtl_file(p->filename, p->value.str) != 0)
                return -1;
        }
        else if (strview_eq_cstr(p->value.str, "usemtl"))
        {
            if (scan_next(p) != TOK_IDENT)
                return parser_error(p, "Expected a material name.\n");
            if (create_submesh(&obj->submeshes, *index_buffer) != 0)
                return -1;
        }
        else if (strview_eq_cstr(p->value.str, "o"))
        {
            if (scan_next(p) != TOK_IDENT)
                return parser_error(p, "Expected an object name.\n");
        }
        else if (strview_eq_cstr(p->value.str, "s"))
        {
            if (scan_next(p) != TOK_INTEGER)
                return parser_error(p, "Expected an integer value.\n");
        }
        else if (strview_eq_cstr(p->value.str, "v"))
        {
            float x, y, z;
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Unexpected token, expected X coordinate.\n");
            x = p->value.float_literal;
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Unexpected token, expected Y coordinate.\n");
            y = p->value.float_literal;
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Unexpected token, expected Z coordinate.\n");
            z = p->value.float_literal;

            if (float_vec_push(obj_v, x) != 0 ||
                float_vec_push(obj_v, y) != 0 || float_vec_push(obj_v, z) != 0)
            {
                return -1;
            }
        }
        else if (strview_eq_cstr(p->value.str, "vn"))
        {
            float x, y, z;
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Unexpected token, expected X normal.\n");
            x = p->value.float_literal;
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Unexpected token, expected Y normal.\n");
            y = p->value.float_literal;
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Unexpected token, expected Z normal.\n");
            z = p->value.float_literal;

            if (float_vec_push(obj_n, x) != 0 ||
                float_vec_push(obj_n, y) != 0 || float_vec_push(obj_n, z) != 0)
            {
                return -1;
            }
        }
        else if (strview_eq_cstr(p->value.str, "vt"))
        {
            float u, v;
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Unexpected token, expected U coordinate.\n");
            u = p->value.float_literal;
            if (scan_next(p) != TOK_FLOAT)
                return parser_error(
                    p, "Unexpected token, expected V coordinate.\n");
            v = p->value.float_literal;

            if (float_vec_push(obj_uv, u) != 0 ||
                float_vec_push(obj_uv, v) != 0)
                return -1;
        }
        else if (strview_eq_cstr(p->value.str, "f"))
        {
            struct gfx_obj_vertex vertex;
            int                   v, vt, vn;
            int                   triangle[3] = {-1, -1, -1};
            int*                  index;
            tok = scan_next(p);
            do
            {
                v = vt = vn = -1;
                if (tok != TOK_INTEGER)
                    return parser_error(
                        p, "Unexpected token, expected vertex index.\n");
                v = p->value.int_literal - 1;
                if (v < 0 || v >= vec_count(*obj_v))
                    return parser_error(p, "Vertex index out of bounds.\n");

                tok = scan_next(p);
                if (tok == '/')
                {
                    tok = scan_next(p);
                    if (tok == TOK_INTEGER)
                    {
                        vt = p->value.int_literal - 1;
                        if (vt < 0 || vt >= vec_count(*obj_uv))
                            return parser_error(
                                p, "Texture index out of bounds.\n");
                        tok = scan_next(p);
                    }
                    if (tok != '/')
                        return parser_error(p, "Missing index.\n");

                    if (scan_next(p) != TOK_INTEGER)
                        return parser_error(p, "Missing normal index.\n");
                    vn = p->value.int_literal - 1;
                    if (vn < 0 || vn >= vec_count(*obj_n))
                        return parser_error(p, "Normal index out of bounds.\n");

                    tok = scan_next(p);
                }

                vertex.pos[0] = *vec_get(*obj_v, v * 3 + 0);
                vertex.pos[1] = *vec_get(*obj_v, v * 3 + 1);
                vertex.pos[2] = *vec_get(*obj_v, v * 3 + 2);

                vertex.uv[0] = *vec_get(*obj_uv, vn * 2 + 0);
                vertex.uv[1] = *vec_get(*obj_uv, vn * 2 + 1);

                switch (index_hmap_emplace_or_get(index_hmap, &vertex, &index))
                {
                    case HMAP_OOM: return -1;
                    case HMAP_NEW:
                        *index = vec_count(*vertex_buffer);
                        if (gfx_obj_vertex_vec_push(vertex_buffer, vertex) != 0)
                            return -1;
                    case HMAP_EXISTS: break;
                }

                if (index_buffer_vec_push(index_buffer, *index) != 0)
                    return -1;

                /* triangulate mesh */
                if (triangle[0] != -1)
                {
                    if (index_buffer_vec_push(index_buffer, triangle[0]) != 0)
                        return -1;
                    if (index_buffer_vec_push(index_buffer, triangle[2]) != 0)
                        return -1;
                }
                triangle[0] = triangle[1];
                triangle[1] = triangle[2];
                triangle[2] = *index;

            } while (tok == TOK_INTEGER);
            goto reswitch_tok;
        }
        else
        {
            return parser_error(p, "Unknown tag\n");
        }
    }

    if (create_submesh(&obj->submeshes, *index_buffer) != 0)
        return -1;

    return 0;
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_obj_init(struct gfx_obj* obj)
{
    obj->vbo = INVALID_HANDLE;
    gfx_obj_submesh_vec_init(&obj->submeshes);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_obj_deinit(struct gfx_obj* obj)
{
    gfx_obj_submesh_vec_deinit(obj->submeshes);
}

/* ------------------------------------------------------------------------- */
int gfx_gles2_obj_load(
    struct gfx_obj*               obj,
    const struct resource_object* res,
    const struct resource_shader* shader)
{
    struct mfile               mf;
    struct parser              p;
    struct float_vec*          obj_v;
    struct float_vec*          obj_n;
    struct float_vec*          obj_uv;
    struct index_buffer_vec*   index_buffer;
    struct gfx_obj_vertex_vec* vertex_buffer;
    struct index_hmap*         index_hmap;

    float_vec_init(&obj_v);
    float_vec_init(&obj_n);
    float_vec_init(&obj_uv);
    index_buffer_vec_init(&index_buffer);
    gfx_obj_vertex_vec_init(&vertex_buffer);
    index_hmap_init(&index_hmap);

    CLITHER_DEBUG_ASSERT(obj->vbo == INVALID_HANDLE);

    log_dbg("Loading obj %s\n", str_cstr(res->obj));
    if (mfile_map_read(&mf, str_cstr(res->obj), 1) != 0)
        goto open_obj_failed;

    parser_init(&p, str_cstr(res->obj), mf.address, mf.size);
    if (parse_obj(
            &p,
            obj,
            &obj_v,
            &obj_n,
            &obj_uv,
            &vertex_buffer,
            &index_buffer,
            &index_hmap) != 0)
    {
        goto parse_obj_failed;
    }
    log_dbg(
        "Loaded %d submeshes, %d vertices\n",
        vec_count(obj->submeshes),
        vec_count(vertex_buffer));

    glGenBuffers(1, &obj->vbo);
    gfx_track_buf(obj->vbo, "obj->vbo");
    glBindBuffer(GL_ARRAY_BUFFER, obj->vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        vec_count(vertex_buffer) * sizeof(struct gfx_obj_vertex),
        vec_data(vertex_buffer),
        GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    mfile_unmap(&mf);

    index_hmap_deinit(index_hmap);
    gfx_obj_vertex_vec_deinit(vertex_buffer);
    index_buffer_vec_deinit(index_buffer);
    float_vec_deinit(obj_uv);
    float_vec_deinit(obj_n);
    float_vec_deinit(obj_v);
    return 0;

parse_obj_failed:
    mfile_unmap(&mf);
open_obj_failed:
    index_hmap_deinit(index_hmap);
    gfx_obj_vertex_vec_deinit(vertex_buffer);
    index_buffer_vec_deinit(index_buffer);
    float_vec_deinit(obj_uv);
    float_vec_deinit(obj_n);
    float_vec_deinit(obj_v);
    return -1;
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_obj_unload(struct gfx_obj* obj)
{
    struct gfx_obj_submesh* mesh;

    vec_for_each (obj->submeshes, mesh)
    {
        gfx_untrack_buf(mesh->ibo);
        glDeleteBuffers(1, &mesh->ibo);
        gfx_untrack_shader(mesh->ibo);
        glDeleteProgram(mesh->program);
    }
    gfx_obj_submesh_vec_clear(obj->submeshes);

    if (obj->vbo != INVALID_HANDLE)
    {
        gfx_untrack_buf(obj->vbo);
        glDeleteBuffers(1, &obj->vbo);
        obj->vbo = INVALID_HANDLE;
    }
}

/* ------------------------------------------------------------------------- */
static void perspective_matrix(
    GLfloat m[4][4],
    GLfloat width,
    GLfloat height,
    GLfloat z_near,
    GLfloat z_far,
    GLfloat fov)
{
    GLfloat ar = width / height;
    GLfloat z_range = z_near - z_far;
    GLfloat tan_half_fov = tanf((fov / 2.0) * M_PI / 180);

    m[0][0] = 1.0f / (tan_half_fov * ar);
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = 0.0f;

    m[1][0] = 0.0f;
    m[1][1] = 1.0f / tan_half_fov;
    m[1][2] = 0.0f;
    m[1][3] = 0.0f;

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = (-z_near - z_far) / z_range;
    m[2][3] = 2.0f * z_far * z_near / z_range;

    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 1.0f;
    m[3][3] = 0.0f;
}

static void zmat(GLfloat m[4][4], GLfloat z_rot)
{
    m[0][0] = cos(z_rot);
    m[0][1] = -sin(z_rot);
    m[0][2] = 0.0f;
    m[0][3] = 0.0f;

    m[1][0] = sin(z_rot);
    m[1][1] = cos(z_rot);
    m[1][2] = 0.0f;
    m[1][3] = 0.0f;

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = 1.0f;
    m[2][3] = 0.0f;

    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}

static void ymat(GLfloat m[4][4], GLfloat y_rot)
{
    m[0][0] = cos(y_rot);
    m[0][1] = 0.0f;
    m[0][2] = sin(y_rot);
    m[0][3] = 0.0f;

    m[1][0] = 0.0f;
    m[1][1] = 1.0f;
    m[1][2] = 0.0f;
    m[1][3] = 0.0f;

    m[2][0] = -sin(y_rot);
    m[2][1] = 0.0f;
    m[2][2] = cos(y_rot);
    m[2][3] = 0.0f;

    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}
static void model_matrix(GLfloat m[4][4], GLfloat x, GLfloat y, GLfloat z)
{
    m[0][0] = 1.0f;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = x;

    m[1][0] = 0.0f;
    m[1][1] = 1.0f;
    m[1][2] = 0.0f;
    m[1][3] = y;

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = 1.0f;
    m[2][3] = z;

    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}

static void
mat_mul(GLfloat out[4][4], const GLfloat m1[4][4], const GLfloat m2[4][4])
{
    int i, j, k;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
        {
            out[i][j] = 0.0f;
            for (k = 0; k < 4; k++)
                out[i][j] += m1[i][k] * m2[k][j];
        }
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_obj_draw(const struct gfx_obj* obj)
{
    const struct gfx_obj_submesh* mesh;
    GLfloat                       model[4][4];
    GLfloat                       view1[4][4];
    GLfloat                       view2[4][4];
    GLfloat                       view[4][4];
    GLfloat                       proj[4][4];
    GLfloat                       mv[4][4];
    GLfloat                       mvp[4][4];
    static GLfloat                zrot, yrot;
    static GLfloat                zpos;

    zrot += M_PI * 0.003;
    yrot += M_PI * 0.005;
    // zpos += 0.01;

    model_matrix(model, 0, 0, zpos);
    zmat(view1, zrot);
    ymat(view2, yrot);
    mat_mul(view, view1, view2);
    perspective_matrix(proj, 640, 480, 1.0, 100, 90);
    mat_mul(mv, model, view);
    mat_mul(mvp, view, proj);

    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);

    glBindBuffer(GL_ARRAY_BUFFER, obj->vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(struct gfx_obj_vertex),
        (void*)offsetof(struct gfx_obj_vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(struct gfx_obj_vertex),
        (void*)offsetof(struct gfx_obj_vertex, uv));

    vec_for_each (obj->submeshes, mesh)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo);
        glUseProgram(mesh->program);
        glUniformMatrix4fv(mesh->sMvp, 1, GL_FALSE, &view[0][0]);
        glDrawElements(GL_TRIANGLES, mesh->index_count, GL_UNSIGNED_INT, NULL);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
