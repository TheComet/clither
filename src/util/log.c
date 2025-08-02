#include "clither/platform/backtrace.h"
#include "clither/util/cli_colors.h"
#include "clither/util/log.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#endif

static CLITHER_THREADLOCAL struct log_interface g_out_log;

#if defined(CLITHER_LOG)
static FILE* g_file_log = NULL;
#endif

enum log_severity
{
    LOG_DBG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERR,
    LOG_NOTE
};

static void default_write_func(const char* fmt, va_list ap)
{
    vfprintf(stderr, fmt, ap);
}
static void default_flush_func(void)
{
    fflush(stderr);
}

#if !defined(_WIN32)
#    include <unistd.h>
#endif
static char stream_is_terminal(FILE* fp)
{
    (void)fp;
#if defined(_WIN32)
    return 1;
#elif defined(__EMSCRIPTEN__)
    return 0;
#else
    return isatty(fileno(fp));
#endif
}

void log_init(void)
{
    g_out_log.write = default_write_func;
    g_out_log.flush = default_flush_func;
    g_out_log.prefix = "";
    g_out_log.set_color = "";
    g_out_log.clear_color = "";
    g_out_log.use_color = stream_is_terminal(stderr);
}

void log_set_prefix(const char* prefix)
{
    g_out_log.prefix = prefix;
}

void log_set_colors(const char* set_color, const char* clear_color)
{
    g_out_log.set_color = set_color;
    g_out_log.clear_color = clear_color;
}

struct log_interface log_configure(struct log_interface iface)
{
    struct log_interface old = g_out_log;
    g_out_log = iface;
    return old;
}

CLITHER_PRINTF_FORMAT(1, 2)
static void out_log_write(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    g_out_log.write(fmt, ap);
    va_end(ap);
}
static void
out_log_vwrite(enum log_severity severity, const char* fmt, va_list ap)
{
    va_list ap_copy;

    /* clang-format off */
    static const char* severity_str[] = {
        "Debug",
        "Info ",
        "Warn ",
        "Error",
        "Note "
    };
    static const char* severity_color[] = {
        COL_N_WHITE,
        COL_B_WHITE,
        COL_B_YELLOW,
        COL_B_RED,
        COL_B_MAGENTA
    };
    /* clang-format on */

    if (g_out_log.write)
    {
        va_copy(ap_copy, ap);
        out_log_write(
            "[%s%s%s] %s%s%s",
            g_out_log.use_color ? severity_color[severity] : "",
            severity_str[severity],
            g_out_log.use_color ? COL_RESET : "",
            g_out_log.set_color,
            g_out_log.prefix,
            g_out_log.clear_color);
        g_out_log.write(fmt, ap_copy);
    }

#if defined(CLITHER_LOG)
    if (g_file_log)
    {
        fprintf(
            g_file_log, "[%s] %s", severity_str[severity], g_out_log.prefix);
        vfprintf(g_file_log, fmt, ap);
        fflush(g_file_log);
    }
#endif
}

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_LOG)
void log_file_open(const char* log_file)
{
    if (g_file_log)
    {
        log_warn(
            "log_file_open() called, but a log file is already open. Closing "
            "previous file...\n");
        log_file_close();
    }

    log_info("Opening log file \"%s\"\n", log_file);
    g_file_log = fopen(log_file, "w");
    if (g_file_log == NULL)
        log_err(
            "Failed to open log file \"%s\": %s\n", log_file, strerror(errno));
}
void log_file_close(void)
{
    if (g_file_log)
    {
        log_info("Closing log file\n");
        fclose(g_file_log);
        g_file_log = NULL;
    }
}
static void log_file_vwrite(const char* fmt, va_list ap)
{
    if (g_file_log)
    {
        fprintf(g_file_log, "%s", g_out_log.prefix);
        vfprintf(g_file_log, fmt, ap);
        fflush(g_file_log);
    }
}
static void log_file_write(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_file_vwrite(fmt, ap);
    va_end(ap);
}
#endif

/* ------------------------------------------------------------------------- */
void log_raw(const char* fmt, ...)
{
    va_list va;

    if (g_out_log.write)
    {
        va_start(va, fmt);
        g_out_log.write(fmt, va);
        va_end(va);
    }

#if defined(CLITHER_LOG)
    if (g_file_log)
    {
        va_start(va, fmt);
        vfprintf(g_file_log, fmt, va);
        va_end(va);
        fflush(g_file_log);
    }
#endif
}

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_LOG_DEBUG)
void log_dbg(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    out_log_vwrite(LOG_DBG, fmt, ap);
    va_end(ap);
}
#endif

/* ------------------------------------------------------------------------- */
void log_info(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    out_log_vwrite(LOG_INFO, fmt, ap);
    va_end(ap);
}

/* ------------------------------------------------------------------------- */
void log_warn(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    out_log_vwrite(LOG_WARN, fmt, ap);
    va_end(ap);
}

/* ------------------------------------------------------------------------- */
int log_err(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    out_log_vwrite(LOG_ERR, fmt, ap);
    va_end(ap);
    return -1;
}

/* ------------------------------------------------------------------------- */
void log_note(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    out_log_vwrite(LOG_NOTE, fmt, ap);
    va_end(ap);
}

/* ------------------------------------------------------------------------- */
#if defined(_WIN32)
int log_err_win32(const char* fmt, ...)
{
    va_list ap;
    char*   error;
    DWORD   dwError = GetLastError();
    if (FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dwError,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&error,
            0,
            NULL) == 0)
    {
        log_err("(Failed to get error from FormatMessage())");
        return -1;
    }

    va_start(ap, fmt);
    out_log_vwrite(LOG_ERR, fmt, ap);
    va_end(ap);
    log_err("(%d) %s", (int)dwError, error);
    LocalFree(error);
    return -1;
}
#endif

/* ------------------------------------------------------------------------- */
int log_oom(int bytes, const char* func_name)
{
    log_err("Failed to allocate %d bytes in %s\n", bytes, func_name);
    log_backtrace();
    return -1;
}

/* ------------------------------------------------------------------------- */
void log_hex_ascii(const void* data, int len)
{
    int i;

    for (i = 0; i != 16; ++i)
        out_log_write("%c  ", "0123456789ABCDEF"[i]);
    out_log_write(" ");
    for (i = 0; i != 16; ++i)
        out_log_write("%c", "0123456789ABCDEF"[i]);
    out_log_write("\n");

    for (i = 0; i < len;)
    {
        int j;
        for (j = 0; j != 16; ++j)
        {
            if (i + j < len)
                out_log_write("%02x ", ((const uint8_t*)data)[i + j]);
            else
                out_log_write("   ");
        }

        out_log_write(" ");
        for (j = 0; j != 16 && i + j != len; ++j)
        {
            uint8_t c = ((const uint8_t*)data)[i + j];
            if (c >= 32 && c < 127) /* printable ascii */
                out_log_write("%c", c);
            else
                out_log_write(".");
        }

        out_log_write("\n");
        i += 16;
    }
}

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_BACKTRACE)
void log_backtrace(void)
{
    char** bt;
    int    bt_size, i;

    if (!(bt = backtrace_get(&bt_size)))
    {
        out_log_write("Failed to generate backtrace\n");
        return;
    }

    for (i = BACKTRACE_OMIT_COUNT; i < bt_size; ++i)
    {
        if (strstr(bt[i], "invoke_main"))
            break;
        out_log_write("  %s\n", bt[i]);
    }
    backtrace_free(bt);
}
#endif

/* ------------------------------------------------------------------------- */
static const char* emph_style(void)
{
    return COL_B_WHITE;
}
static const char* error_style(void)
{
    return COL_B_RED;
}
static const char* underline_style(void)
{
    return COL_B_RED;
}
static const char* reset_style(void)
{
    return COL_RESET;
}

/* ------------------------------------------------------------------------- */
void log_vflc(
    const char*    filename,
    const char*    source,
    struct strspan loc,
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

    out_log_write(
        "%s%s:%d:%d:%s ", emph_style(), filename, l1, c1, reset_style());
    out_log_write("%serror:%s ", error_style(), reset_style());
    g_out_log.write(fmt, ap);
}

/* ------------------------------------------------------------------------- */
void log_flc(
    const char*    filename,
    const char*    source,
    struct strspan loc,
    const char*    fmt,
    ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_vflc(filename, source, loc, fmt, ap);
    va_end(ap);
}

/* ------------------------------------------------------------------------- */
void log_excerpt(const char* source, struct strspan loc)
{
    int            i;
    int            l1, c1, l2, c2;
    int            indent, max_indent;
    int            gutter_indent;
    int            line;
    struct strspan block;

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
        out_log_write("%*d | ", gutter_indent - 1, line);

        if (i >= loc.off - block.off && i <= loc.off - block.off + loc.len)
            out_log_write("%s", underline_style());

        indent = 0;
        while (i != block.len)
        {
            if (i == loc.off - block.off)
                out_log_write("%s", underline_style());
            if (i == loc.off - block.off + loc.len)
                out_log_write("%s", reset_style());

            if (indent++ >= max_indent)
                out_log_write("%c", source[block.off + i]);

            if (source[block.off + i++] == '\n')
            {
                if (i >= loc.off - block.off &&
                    i <= loc.off - block.off + loc.len)
                    out_log_write("%s", reset_style());
                break;
            }
        }
        line++;
    }
    out_log_write("%s\n", reset_style());

    /* print underline */
    if (c2 > c1)
    {
        out_log_write("%*s|%*s", gutter_indent, "", c1, "");
        out_log_write("%s", underline_style());
        out_log_write("%c", '^');
        for (i = c1 + 1; i < c2; ++i)
            out_log_write("%c", '~');
        out_log_write("%s", reset_style());
    }
    else
    {
        int col, max_col;

        out_log_write("%*s| ", gutter_indent, "");
        out_log_write("%s", underline_style());
        for (i = 1; i < c2; ++i)
            out_log_write("%c", '~');
        for (; i < c1; ++i)
            out_log_write("%c", ' ');
        out_log_write("%c", '^');

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
            out_log_write("%c", '~');
        out_log_write("%s", reset_style());
    }

    out_log_write("%c", '\n');
}
