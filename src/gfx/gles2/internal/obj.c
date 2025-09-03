#include "./obj.h"
#include "clither/game/resource_pack.h"
#include "clither/platform/mfile.h"
#include <ctype.h>

VEC_DEFINE(gfx_obj_tex_vec, struct gfx_obj_tex, 8)
VEC_DEFINE(gfx_obj_mat_vec, struct gfx_obj_mat, 8)
VEC_DEFINE(gfx_obj_vertex_vec, struct gfx_obj_vertex, 32)
VEC_DECLARE(float_vec, float, 32)
VEC_DEFINE(float_vec, float, 32)

enum token
{
    TOK_ERROR = -1,
    TOK_END = 0,
    TOK_SLASH = '/',
    TOK_TAG = 256,
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
    fprintf(stderr, "%s[C-INI] error:%s ", error_style(), reset_style());
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
                    p->data[p->head] == '.'))
            {
                p->head++;
            }
            p->value.str.len = p->head - p->value.str.off;
            return TOK_TAG;
        }

        p->tail = ++p->head;
    }

    return TOK_END;
}

/* ------------------------------------------------------------------------- */
static int parse_obj(
    struct parser*              p,
    struct float_vec**          vertices,
    struct float_vec**          normals,
    struct float_vec**          uvs,
    struct gfx_obj_vertex_vec** attrs)
{
    while (1)
    {
        enum token tok = scan_next(p);
        if (tok == TOK_END || tok == TOK_ERROR)
            return tok;

        if (tok != TOK_TAG)
            continue;
        if (strview_eq_cstr(p->value.str, "v"))
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

            if (float_vec_push(vertices, x) != 0 ||
                float_vec_push(vertices, y) != 0 ||
                float_vec_push(vertices, z) != 0)
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

            if (float_vec_push(normals, x) != 0 ||
                float_vec_push(normals, y) != 0 ||
                float_vec_push(normals, z) != 0)
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

            if (float_vec_push(uvs, u) != 0 || float_vec_push(uvs, v) != 0)
                return -1;
        }
        else if (strview_eq_cstr(p->value.str, "f"))
        {
            struct gfx_obj_vertex* attr;
            int                    v = -1, vt = -1, vn = -1;
            if (scan_next(p) != TOK_INTEGER)
                return parser_error(
                    p, "Unexpected token, expected vertex index.\n");
            v = p->value.int_literal - 1;
            if (v < 0 || v >= vec_count(*vertices))
                return parser_error(p, "Vertex index out of bounds.\n");

            tok = scan_next(p);
            if (tok == '/')
            {
                tok = scan_next(p);
                if (tok == TOK_INTEGER)
                {
                    vt = p->value.int_literal - 1;
                    if (vt < 0 || vt >= vec_count(*uvs))
                        return parser_error(
                            p, "Texture index out of bounds.\n");
                    tok = scan_next(p);
                }
                if (tok != '/')
                    return parser_error(p, "Missing index.\n");
                if (scan_next(p) != TOK_INTEGER)
                    return parser_error(p, "Missing normal index.\n");
                vn = p->value.int_literal - 1;
                if (vn < 0 || vn >= vec_count(*normals))
                    return parser_error(p, "Normal index out of bounds.\n");
            }

            attr = gfx_obj_vertex_vec_emplace(attrs);
            if (attr == NULL)
                return -1;

            attr->pos[0] = *vec_get(*vertices, v * 3 + 0);
            attr->pos[1] = *vec_get(*vertices, v * 3 + 1);
            attr->pos[2] = *vec_get(*vertices, v * 3 + 2);

            attr->uv[0] = *vec_get(*uvs, vn * 2 + 0);
            attr->uv[1] = *vec_get(*uvs, vn * 2 + 1);
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_obj_init(struct gfx_obj* obj)
{
    obj->mesh.vbo = INVALID_HANDLE;
    gfx_obj_tex_vec_init(&obj->tex);
    gfx_obj_mat_vec_init(&obj->mat);
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_obj_deinit(struct gfx_obj* obj)
{
    gfx_obj_mat_vec_deinit(obj->mat);
    gfx_obj_tex_vec_deinit(obj->tex);
}

/* ------------------------------------------------------------------------- */
int gfx_gles2_obj_load(
    struct gfx_obj*               obj,
    const struct resource_object* res,
    const struct resource_shader* shader)
{
    struct mfile               mf;
    struct parser              p;
    struct gfx_obj_vertex_vec* attrs;
    struct float_vec*          vertices;
    struct float_vec*          normals;
    struct float_vec*          uvs;

    gfx_obj_vertex_vec_init(&attrs);
    float_vec_init(&vertices);
    float_vec_init(&normals);
    float_vec_init(&uvs);

    CLITHER_DEBUG_ASSERT(obj->mesh.vbo == INVALID_HANDLE);

    log_dbg("Loading obj %s\n", str_cstr(res->obj));
    if (mfile_map_read(&mf, str_cstr(res->obj), 1) != 0)
        goto open_obj_failed;

    parser_init(&p, str_cstr(res->obj), mf.address, mf.size);
    if (parse_obj(&p, &vertices, &normals, &uvs, &attrs) != 0)
        goto parse_obj_failed;

    mfile_unmap(&mf);
    float_vec_deinit(uvs);
    float_vec_deinit(normals);
    float_vec_deinit(vertices);
    gfx_obj_vertex_vec_deinit(attrs);
    return 0;

parse_obj_failed:
    mfile_unmap(&mf);
open_obj_failed:
    float_vec_deinit(uvs);
    float_vec_deinit(normals);
    float_vec_deinit(vertices);
    gfx_obj_vertex_vec_deinit(attrs);
    return -1;
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_obj_unload(struct gfx_obj* obj)
{
}

/* ------------------------------------------------------------------------- */
void gfx_gles2_obj_draw(const struct gfx_obj* ubj)
{
}
