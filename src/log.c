#include "clither/cli_colors.h"
#include "clither/log.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

static CLITHER_THREADLOCAL const char* g_prefix = "";
static CLITHER_THREADLOCAL const char* g_col_set = "";
static CLITHER_THREADLOCAL const char* g_col_clr = "";

#if defined(CLITHER_LOGGING)
static FILE* g_log = NULL;
static FILE* g_net = NULL;
#endif

/* ------------------------------------------------------------------------- */
void log_set_prefix(const char* prefix)
{
    g_prefix = prefix;
}
void log_set_colors(const char* set, const char* clear)
{
    g_col_set = set;
    g_col_clr = clear;
}

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_LOGGING)
void log_file_open(const char* log_file)
{
    if (g_log)
    {
        log_warn(
            "log_file_open() called, but a log file is already open. Closing "
            "previous file...\n");
        log_file_close();
    }

    log_info("Opening log file \"%s\"\n", log_file);
    g_log = fopen(log_file, "w");
    if (g_log == NULL)
        log_err(
            "Failed to open log file \"%s\": %s\n", log_file, strerror(errno));
}
void log_file_close(void)
{
    if (g_log)
    {
        log_info("Closing log file\n");
        fclose(g_log);
        g_log = NULL;
    }
}

void log_net_open(const char* log_file)
{
    if (g_net)
    {
        log_warn(
            "log_net_open() called, but a log file is already open. Closing "
            "previous file...\n");
        log_net_close();
    }

    log_info("Opening networking log file \"%s\"\n", log_file);
    g_net = fopen(log_file, "w");
    if (g_net == NULL)
        log_err(
            "Failed to open log file \"%s\": %s\n", log_file, strerror(errno));
}
void log_net_close(void)
{
    if (g_net)
    {
        log_info("Closing net log file\n");
        fclose(g_net);
        g_net = NULL;
    }
}
#endif

/* ------------------------------------------------------------------------- */
void log_raw(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);

#if defined(CLITHER_LOGGING)
    if (g_log)
    {
        va_start(va, fmt);
        vfprintf(g_log, fmt, va);
        va_end(va);
        fflush(g_log);
    }
#endif
}

/* ------------------------------------------------------------------------- */
void log_dbg(const char* fmt, ...)
{
    va_list va;
    fprintf(
        stderr,
        "[" COL_N_YELLOW "Debug" COL_RESET "] %s%s%s",
        g_col_set,
        g_prefix,
        g_col_clr);
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);

#if defined(CLITHER_LOGGING)
    if (g_log)
    {
        fprintf(g_log, "[Debug] %s", g_prefix);
        va_start(va, fmt);
        vfprintf(g_log, fmt, va);
        va_end(va);
        fflush(g_log);
    }
#endif
}

/* ------------------------------------------------------------------------- */
void log_info(const char* fmt, ...)
{
    va_list va;
    fprintf(
        stderr,
        "[" COL_B_WHITE "Info " COL_RESET "] %s%s%s",
        g_col_set,
        g_prefix,
        g_col_clr);
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);

#if defined(CLITHER_LOGGING)
    if (g_log)
    {
        fprintf(g_log, "[Info ] %s", g_prefix);
        va_start(va, fmt);
        vfprintf(g_log, fmt, va);
        va_end(va);
        fflush(g_log);
    }
#endif
}

/* ------------------------------------------------------------------------- */
void log_warn(const char* fmt, ...)
{
    va_list va;
    fprintf(
        stderr,
        "[" COL_B_YELLOW "Warn " COL_RESET "] %s%s%s",
        g_col_set,
        g_prefix,
        g_col_clr);
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);

#if defined(CLITHER_LOGGING)
    if (g_log)
    {
        fprintf(g_log, "[Warn ] %s", g_prefix);
        va_start(va, fmt);
        vfprintf(g_log, fmt, va);
        va_end(va);
        fflush(g_log);
    }
#endif
}

/* ------------------------------------------------------------------------- */
void log_err(const char* fmt, ...)
{
    va_list va;
    fprintf(
        stderr,
        "[" COL_B_RED "Error" COL_RESET "] %s%s%s",
        g_col_set,
        g_prefix,
        g_col_clr);
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);

#if defined(CLITHER_LOGGING)
    if (g_log)
    {
        fprintf(g_log, "[Error] %s", g_prefix);
        va_start(va, fmt);
        vfprintf(g_log, fmt, va);
        va_end(va);
        fflush(g_log);
    }
#endif
}

/* ------------------------------------------------------------------------- */
void log_note(const char* fmt, ...)
{
    va_list va;
    fprintf(
        stderr,
        "[" COL_B_MAGENTA "NOTE " COL_RESET "] %s%s%s" COL_B_YELLOW,
        g_col_set,
        g_prefix,
        g_col_clr);
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    fprintf(stderr, COL_RESET);

#if defined(CLITHER_LOGGING)
    if (g_log)
    {
        fprintf(g_log, "[NOTE ] %s", g_prefix);
        va_start(va, fmt);
        vfprintf(g_log, fmt, va);
        va_end(va);
        fflush(g_log);
    }
#endif
}

/* ------------------------------------------------------------------------- */
void log_net(const char* fmt, ...)
{
#if defined(CLITHER_LOGGING)
    if (g_net)
    {
        va_list va;
        fprintf(g_net, "%s", g_prefix);
        va_start(va, fmt);
        vfprintf(g_net, fmt, va);
        va_end(va);
        fflush(g_net);
    }
#endif
}

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

    fprintf(
        stderr,
        "%s%s:%d:%d:%s ",
        emph_style(),
        filename,
        l1,
        c1,
        reset_style());
    fprintf(stderr, "%serror:%s ", error_style(), reset_style());
    vfprintf(stderr, fmt, ap);
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
                if (i >= loc.off - block.off
                    && i <= loc.off - block.off + loc.len)
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
