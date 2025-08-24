#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <Windows.h>
#else
#    define _GNU_SOURCE
#    include <errno.h>
#    include <sys/fcntl.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

#include <ctype.h>
#include <float.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------------------
 * Printing and formatting
 * ------------------------------------------------------------------------- */

/*! All strings are represented as an offset and a length into a buffer. */
struct strview
{
    const char* source;
    int         off, len;
};

static struct strview cstr_strview(const char* str)
{
    struct strview sv;
    sv.source = str;
    sv.off = 0;
    sv.len = strlen(str);
    return sv;
}

static struct strview empty_strview(void)
{
    struct strview sv;
    sv.source = "";
    sv.off = 0;
    sv.len = 0;
    return sv;
}

static int cstr_equal(const char* s1, struct strview s2)
{
    if ((int)strlen(s1) != s2.len)
        return 0;
    return memcmp(s1, s2.source + s2.off, s2.len) == 0;
}

static int disable_colors = 0;

static const char* emph_style(void)
{
    return disable_colors ? "" : "\033[1;37m";
}
static const char* error_style(void)
{
    return disable_colors ? "" : "\033[1;31m";
}
static const char* underline_style(void)
{
    return disable_colors ? "" : "\033[1;31m";
}
static const char* reset_style(void)
{
    return disable_colors ? "" : "\033[0m";
}

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

static int print_error(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%serror:%s ", error_style(), reset_style());
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    return -1;
}

/* ----------------------------------------------------------------------------
 * Platform abstractions & Utilities
 * ------------------------------------------------------------------------- */

/*! Used to disable colors if the stream is being redirected */
#if defined(WIN32)
static int stream_is_terminal(FILE* fp)
{
    return 1;
}
#else
static int stream_is_terminal(FILE* fp)
{
    return isatty(fileno(fp));
}
#endif

/*! Memory-mapped file */
struct mfile
{
    void* address;
    int   size;
};

#if defined(WIN32)
static void print_last_win32_error(void)
{
    LPSTR  msg;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&msg,
        0,
        NULL);
    print_error("%.*s", (int)size, msg);
    LocalFree(msg);
}
static wchar_t* utf8_to_utf16(const char* utf8, int utf8_bytes)
{
    int utf16_bytes =
        MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_bytes, NULL, 0);
    if (utf16_bytes == 0)
        return NULL;

    wchar_t* utf16 = malloc((sizeof(wchar_t) + 1) * utf16_bytes);
    if (utf16 == NULL)
        return NULL;

    if (MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_bytes, utf16, utf16_bytes) ==
        0)
    {
        free(utf16);
        return NULL;
    }

    utf16[utf16_bytes] = 0;

    return utf16;
}
static void utf_free(void* utf)
{
    free(utf);
}
#endif

/*!
 * \brief Memory-maps a file in read-only mode.
 * \param[in] mf Pointer to mfile structure. Struct can be uninitialized.
 * \param[in] file_path Utf8 encoded file path.
 * \param[in] silence_open_error If zero, an error is printed to stderr if
 * mapping fails. If one, no errors are printed. When comparing the generated
 * code with an already existing file, we allow the function to fail silently if
 * the file does not exist. \return Returns 0 on success, negative on failure.
 */
static int
mfile_map_read(struct mfile* mf, const char* file_path, int silence_open_error)
{
#if defined(WIN32)
    HANDLE        hFile;
    LARGE_INTEGER liFileSize;
    HANDLE        mapping;
    wchar_t*      utf16_file_path;

    utf16_file_path = utf8_to_utf16(file_path, (int)strlen(file_path));
    if (utf16_file_path == NULL)
        goto utf16_conv_failed;

    /* Try to open the file */
    hFile = CreateFileW(
        utf16_file_path, /* File name */
        GENERIC_READ,    /* Read only */
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,                  /* Default security */
        OPEN_EXISTING,         /* File must exist */
        FILE_ATTRIBUTE_NORMAL, /* Default attributes */
        NULL);                 /* No attribute template */
    if (hFile == INVALID_HANDLE_VALUE)
    {
        if (!silence_open_error)
            print_last_win32_error();
        goto open_failed;
    }

    /* Determine file size in bytes */
    if (!GetFileSizeEx(hFile, &liFileSize))
        goto get_file_size_failed;
    if (liFileSize.QuadPart > (1ULL << 32) - 1) /* mf->size is an int */
        goto get_file_size_failed;

    mapping = CreateFileMapping(
        hFile,         /* File handle */
        NULL,          /* Default security attributes */
        PAGE_READONLY, /* Read only (or copy on write, but we don't write) */
        0,
        0,     /* High/Low size of mapping. Zero means entire file */
        NULL); /* Don't name the mapping */
    if (mapping == NULL)
    {
        print_last_win32_error();
        goto create_file_mapping_failed;
    }

    mf->address = MapViewOfFile(
        mapping,       /* File mapping handle */
        FILE_MAP_READ, /* Read-only view of file */
        0,
        0,  /* High/Low offset of where the mapping should begin in the file */
        0); /* Length of mapping. Zero means entire file */
    if (mf->address == NULL)
    {
        print_last_win32_error();
        goto map_view_failed;
    }

    /* The file mapping isn't required anymore */
    CloseHandle(mapping);
    CloseHandle(hFile);
    utf_free(utf16_file_path);

    mf->size = (int)liFileSize.QuadPart;

    return 0;

map_view_failed:
    CloseHandle(mapping);
create_file_mapping_failed:
get_file_size_failed:
    CloseHandle(hFile);
open_failed:
    utf_free(utf16_file_path);
utf16_conv_failed:
    return -1;
#else
    struct stat stbuf;
    int         fd;

    fd = open(file_path, O_RDONLY);
    if (fd < 0)
    {
        if (!silence_open_error)
            print_error(
                "Failed to open file \"%s\": %s\n", file_path, strerror(errno));
        goto open_failed;
    }

    if (fstat(fd, &stbuf) != 0)
    {
        print_error(
            "Failed to stat file \"%s\": %s\n", file_path, strerror(errno));
        goto fstat_failed;
    }
    if (!S_ISREG(stbuf.st_mode))
    {
        print_error("File \"%s\" is not a regular file!\n", file_path);
        goto fstat_failed;
    }

    mf->address = mmap(
        NULL,
        (size_t)stbuf.st_size,
        PROT_READ,
        MAP_PRIVATE | MAP_NORESERVE,
        fd,
        0);
    if (mf->address == MAP_FAILED)
    {
        print_error(
            "Failed to mmap() file \"%s\": %s\n", file_path, strerror(errno));
        goto mmap_failed;
    }

    /* file descriptor no longer required */
    close(fd);

    mf->size = (int)stbuf.st_size;
    return 0;

mmap_failed:
fstat_failed:
    close(fd);
open_failed:
    return -1;
#endif
}

/*!
 * \brief Memory-maps a file in read-write mode.
 * \param[in] mf Pointer to mfile structure. Struct can be uninitialized.
 * \param[in] file_path Utf8 encoded file path.
 * \param[in] size Size of the file in bytes. This is used to allocate space
 * on the file system.
 * \return Returns 0 on success, negative on failure.
 */
static int mfile_map_write(struct mfile* mf, const char* file_path, int size)
{
#if defined(WIN32)
    HANDLE   hFile;
    HANDLE   mapping;
    wchar_t* utf16_file_path;

    utf16_file_path = utf8_to_utf16(file_path, (int)strlen(file_path));
    if (utf16_file_path == NULL)
        goto utf16_conv_failed;

    /* Try to open the file */
    hFile = CreateFileW(
        utf16_file_path,              /* File name */
        GENERIC_READ | GENERIC_WRITE, /* Read/write */
        0,
        NULL,                  /* Default security */
        CREATE_ALWAYS,         /* Overwrite any existing, otherwise create */
        FILE_ATTRIBUTE_NORMAL, /* Default attributes */
        NULL);                 /* No attribute template */
    if (hFile == INVALID_HANDLE_VALUE)
    {
        print_last_win32_error();
        goto open_failed;
    }

    mapping = CreateFileMappingW(
        hFile,          /* File handle */
        NULL,           /* Default security attributes */
        PAGE_READWRITE, /* Read + Write */
        0,
        size,  /* High/Low size of mapping */
        NULL); /* Don't name the mapping */
    if (mapping == NULL)
    {
        print_last_win32_error();
        goto create_file_mapping_failed;
    }

    mf->address = MapViewOfFile(
        mapping,                        /* File mapping handle */
        FILE_MAP_READ | FILE_MAP_WRITE, /* Read + Write */
        0,
        0,  /* High/Low offset of where the mapping should begin in the file */
        0); /* Length of mapping. Zero means entire file */
    if (mf->address == NULL)
    {
        print_last_win32_error();
        goto map_view_failed;
    }

    /* The file mapping isn't required anymore */
    CloseHandle(mapping);
    CloseHandle(hFile);
    utf_free(utf16_file_path);

    mf->size = size;

    return 0;

map_view_failed:
    CloseHandle(mapping);
create_file_mapping_failed:
    CloseHandle(hFile);
open_failed:
    utf_free(utf16_file_path);
utf16_conv_failed:
    return -1;
#else
    int fd = open(
        file_path,
        O_CREAT | O_RDWR | O_TRUNC,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0)
    {
        print_error(
            "Failed to open file \"%s\" for writing: %s\n",
            file_path,
            strerror(errno));
        goto open_failed;
    }

    /* When truncating the file, it must be expanded again, otherwise writes to
     * the memory will cause SIGBUS.
     * NOTE: If this ever gets ported to non-Linux, see posix_fallocate() */
    if (fallocate(fd, 0, 0, size) != 0)
    {
        print_error(
            "Failed to resize file \"%s\" to %d: %s\n",
            file_path,
            size,
            strerror(errno));
        goto mmap_failed;
    }

    mf->address =
        mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mf->address == MAP_FAILED)
    {
        print_error(
            "Failed to mmap() file \"%s\" for writing: %s\n",
            file_path,
            strerror(errno));
        goto mmap_failed;
    }

    /* file descriptor no longer required */
    close(fd);

    mf->size = size;
    return 0;

mmap_failed:
    close(fd);
open_failed:
    return -1;
#endif
}

static int mfile_map_mem(struct mfile* mf, int size)
{
#if defined(WIN32)
    HANDLE mapping = CreateFileMapping(
        INVALID_HANDLE_VALUE, /* File handle */
        NULL,                 /* Default security attributes */
        PAGE_READWRITE,       /* Read + Write access */
        0,
        size,  /* High/Low size of mapping. Zero means entire file */
        NULL); /* Don't name the mapping */
    if (mapping == NULL)
    {
        print_error(
            "Failed to create file mapping of size %d: {win32error}\n", size);
        goto create_file_mapping_failed;
    }

    mf->address = MapViewOfFile(
        mapping,        /* File mapping handle */
        FILE_MAP_WRITE, /* Read + Write */
        0,
        0, /* High/Low offset of where the mapping should begin in the file */
        size); /* Length of mapping. Zero means entire file */
    if (mf->address == NULL)
    {
        print_error(
            "Failed to map memory of size {emph:%d}: {win32error}\n", size);
        goto map_view_failed;
    }

    CloseHandle(mapping);
    mf->size = size;

    return 0;

map_view_failed:
    CloseHandle(mapping);
create_file_mapping_failed:
    return -1;
#else
    mf->address = mmap(
        NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mf->address == MAP_FAILED)
        return print_error(
            "Failed to mmap() {emph:%d} bytes: %s\n", size, strerror(errno));

    mf->size = size;
    return 0;
#endif
}

/*!
 * \brief Unmaps a previously memory-mapped file.
 * \param mf Pointer to mfile structure.
 */
void mfile_unmap(struct mfile* mf)
{
#if defined(WIN32)
    UnmapViewOfFile(mf->address);
#else
    munmap(mf->address, (size_t)mf->size);
#endif
}

static int mfile_map_stdin(struct mfile* mf)
{
    char         b;
    int          len;
    struct mfile new_mf;
    mf->size = 0;

    len = 0;
    while ((b = getc(stdin)) != EOF)
    {
        if (len >= mf->size)
        {
            if (mfile_map_mem(&new_mf, mf->size ? mf->size * 2 : 1024 * 1024) !=
                0)
            {
                return -1;
            }
            memcpy(new_mf.address, mf->address, mf->size);
            if (mf->size)
                mfile_unmap(mf);
            *mf = new_mf;
        }
        ((char*)mf->address)[len++] = b;
    }

    if (len == 0)
        return print_error("Input is empty\n");

    if (mfile_map_mem(&new_mf, len) != 0)
        return -1;
    memcpy(mf->address, new_mf.address, len);
    mfile_unmap(mf);
    *mf = new_mf;

    return 0;
}

static int file_is_source_file(const char* filename)
{
    int len = strlen(filename);
    return strcmp(&filename[len - 2], ".c") == 0 ||
           strcmp(&filename[len - 4], ".cpp") == 0 ||
           strcmp(&filename[len - 4], ".cxx") == 0 ||
           strcmp(&filename[len - 3], ".cc") == 0;
}

/*! A memory buffer that grows as data is added. */
struct mstream
{
    void* address;
    int   capacity;
    int   write_ptr;
};

/*! Init a new mstream structure ready for writing */
static struct mstream mstream_init_writeable(void)
{
    struct mstream ms;
    ms.address = NULL;
    ms.capacity = 0;
    ms.write_ptr = 0;
    return ms;
}

/*!
 * \brief Grows the capacity of mstream if required
 * \param[in] ms Memory stream structure.
 * \param[in] additional_size How many bytes to grow the capacity of the buffer
 * by.
 */
static void mstream_grow(struct mstream* ms, int additional_size)
{
    while (ms->capacity < ms->write_ptr + additional_size)
    {
        ms->capacity = ms->capacity == 0 ? 32 : ms->capacity * 2;
        ms->address = realloc(ms->address, ms->capacity);
    }
}

/*! Write a single character (byte) to the mstream buffer */
static void mstream_putc(struct mstream* ms, char c)
{
    mstream_grow(ms, 1);
    ((char*)ms->address)[ms->write_ptr++] = c;
}

/*! Convert an integer to a string and write it to the mstream buffer */
static void mstream_write_int(struct mstream* ms, int in)
{
    int     digit = 1000000000;
    int64_t value = in;
    mstream_grow(ms, sizeof("-2147483648") - 1);
    if (value < 0)
    {
        ((char*)ms->address)[ms->write_ptr++] = '-';
        value = -value;
    }
    else if (value == 0)
    {
        ((char*)ms->address)[ms->write_ptr++] = '0';
        return;
    }
    while (value < digit)
        digit /= 10;
    while (digit)
    {
        ((char*)ms->address)[ms->write_ptr] = '0';
        while (value >= digit)
        {
            value -= digit;
            ((char*)ms->address)[ms->write_ptr]++;
        }
        ms->write_ptr++;
        digit /= 10;
    }
}

static void mstream_write_float(struct mstream* ms, double value)
{
    mstream_grow(ms, 32);
    ms->write_ptr += sprintf((char*)ms->address + ms->write_ptr, "%.9g", value);
}
/*! Write a C-string to the mstream buffer */
static void mstream_cstr(struct mstream* ms, const char* cstr)
{
    int len = (int)strlen(cstr);
    mstream_grow(ms, len);
    memcpy((char*)ms->address + ms->write_ptr, cstr, len);
    ms->write_ptr += len;
}

/*! Write a string view to the mstream buffer */
static void mstream_str(struct mstream* ms, struct strview str)
{
    mstream_grow(ms, str.len);
    memcpy((char*)ms->address + ms->write_ptr, str.source + str.off, str.len);
    ms->write_ptr += str.len;
}

/*!
 * \brief Write a formatted string to the mstream buffer.
 * This function is similar to printf() but only implements a subset of the
 * format specifiers. These are:
 *   %i - Write an integer (int)
 *   %d - Write an integer (int)
 *   %s - Write a c-string (const char*)
 *   %S - Write a string view (struct strview, const char*)
 * \param[in] ms Pointer to mstream structure.
 * \param[in] fmt Format string.
 * \param[in] ... Additional parameters.
 */
static void mstream_fmt(struct mstream* ms, const char* fmt, ...)
{
    int     i;
    va_list va;
    va_start(va, fmt);
    for (i = 0; fmt[i]; ++i)
    {
        if (fmt[i] == '%')
            switch (fmt[++i])
            {
                case 'c': mstream_putc(ms, (char)va_arg(va, int)); continue;
                case 's': mstream_cstr(ms, va_arg(va, const char*)); continue;
                case 'i':
                case 'd': mstream_write_int(ms, va_arg(va, int)); continue;
                case 'f': mstream_write_float(ms, va_arg(va, double)); continue;
                case 'S': {
                    struct strview str = va_arg(va, struct strview);
                    mstream_str(ms, str);
                    continue;
                }
            }
        mstream_putc(ms, fmt[i]);
    }
    va_end(va);
}

/* ----------------------------------------------------------------------------
 * Settings & Command Line
 * ------------------------------------------------------------------------- */

struct cfg
{
    const char* output_header;
    const char* output_source;
    char**      input_fnames;
    int         input_count;
    char**      include_files;
    int         include_file_count;
};

static int print_help(const char* prog_name)
{
    fprintf(
        stderr,
        "Usage: %s <args>\n"
        "  --input <files...>\n"
        "  --include-files <additional files to include...>\n"
        "  --output-header <filename.h>\n"
        "  --output-source <filename.c>\n",
        prog_name);
    return 1;
}

static int parse_cmdline(int argc, char** argv, struct cfg* cfg)
{
    int i;
    for (i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            return print_help(argv[0]);
        else if (strcmp(argv[i], "--input") == 0)
        {
            if (i + 1 >= argc)
                return print_error(
                    "Missing input filename(s) to option --input\n");
            cfg->input_fnames = &argv[i + 1];
            cfg->input_count = 0;
            while (i + 1 < argc && argv[i + 1][0] != '-')
                ++i, ++cfg->input_count;
            if (cfg->input_count == 0)
                return print_error("No input files specified\n");
        }
        else if (strcmp(argv[i], "--include-files") == 0)
        {
            if (i + 1 >= argc)
                return print_error(
                    "Missing include dir(s) to option --include-files\n");
            cfg->include_files = &argv[i + 1];
            cfg->include_file_count = 0;
            while (i + 1 < argc && argv[i + 1][0] != '-')
                ++i, ++cfg->include_file_count;
            if (cfg->include_file_count == 0)
                return print_error("No include directories specified\n");
        }
        else if (strcmp(argv[i], "--output-header") == 0)
        {
            if (++i >= argc)
                return print_error(
                    "Missing output header filename to option "
                    "--output-header\n");
            cfg->output_header = argv[i];
        }
        else if (strcmp(argv[i], "--output-source") == 0)
        {
            if (++i >= argc)
                return print_error(
                    "Missing output source filename to option "
                    "--output-source\n");
            cfg->output_source = argv[i];
        }
        else
        {
            print_error("Unknown option \"%s\"\n", argv[i]);
            return print_help(argv[0]);
        }
    }

    return 0;
}

/* ----------------------------------------------------------------------------
 * Parser
 * ------------------------------------------------------------------------- */

#define CHAR_TOKEN_LIST                                                        \
    X(LBRACE, '{')                                                             \
    X(RBRACE, '}')                                                             \
    X(LBRACKET, '[')                                                           \
    X(RBRACKET, ']')                                                           \
    X(LPAREN, '(')                                                             \
    X(RPAREN, ')')                                                             \
    X(EQUALS, '=')                                                             \
    X(COMMA, ',')                                                              \
    X(COLON, ':')                                                              \
    X(ASTERISK, '*')                                                           \
    X(SEMICOLON, ';')

enum token
{
    TOK_ERROR = -1,
    TOK_END = 0,
#define X(name, char) TOK_##name = char,
    CHAR_TOKEN_LIST
#undef X
        TOK_IDENTIFIER = 256,
    TOK_STRING,
    TOK_INTEGER,
    TOK_FLOAT
};

struct parser
{
    union
    {
        struct strview str;
        int64_t        integer;
        double         floating;
    } value;
    const char* filename;
    const char* data;
    int         tail;
    int         head;
    int         end;
};

static void
parser_init(struct parser* p, const struct mfile* mf, const char* filename)
{
    p->filename = filename;
    p->data = (char*)mf->address;
    p->end = mf->size;
    p->head = 0;
    p->tail = 0;
}

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

enum token scan_next(struct parser* p)
{
    p->tail = p->head;
    while (p->head != p->end)
    {
        if (p->data[p->head] == '/' && p->data[p->head + 1] == '*')
        {
            /* Find end of comment */
            for (p->head += 2; p->head != p->end; p->head++)
                if (p->data[p->head] == '*' && p->data[p->head + 1] == '/')
                {
                    p->head += 2;
                    break;
                }
            if (p->head == p->end)
                return parser_error(p, "Missing closing comment\n");
        }
        if (p->data[p->head] == '/' && p->data[p->head + 1] == '/')
        {
            /* Find end of comment */
            for (p->head += 2; p->head != p->end; p->head++)
                if (p->data[p->head] == '\n')
                {
                    p->head++;
                    break;
                }
            if (p->head == p->end)
                return parser_error(p, "Missing closing comment\n");
        }

        /* Single characters */
#define X(name, char)                                                          \
    if (p->data[p->head] == char)                                              \
        return p->data[p->head++];
        CHAR_TOKEN_LIST
#undef X

        /* Hexadecimal */
        if (p->end - p->head > 2 && p->data[p->head] == '0' &&
            (p->data[p->head + 1] == 'x' || p->data[p->head + 1] == 'X'))
        {
            p->head += 2;
            p->value.integer = 0;
            for (; p->head != p->end &&
                   (isdigit(p->data[p->head]) ||
                    (p->data[p->head] >= 'A' && p->data[p->head] <= 'F') ||
                    (p->data[p->head] >= 'a' && p->data[p->head] <= 'f'));
                 ++p->head)
            {
                p->value.integer *= 16;
                if (isdigit(p->data[p->head]))
                    p->value.integer += p->data[p->head] - '0';
                else if (p->data[p->head] >= 'A' && p->data[p->head] <= 'F')
                    p->value.integer += p->data[p->head] - 'A';
                else
                    p->value.integer += p->data[p->head] - 'a';
            }

            return TOK_INTEGER;
        }

        /* Number */
        if (isdigit(p->data[p->head]) || p->data[p->head] == '-')
        {
            char is_neg = p->data[p->head] == '-';
            if (p->data[p->head] == '-')
                p->head++;

            p->value.integer = 0;
            for (; p->head != p->end && isdigit(p->data[p->head]); ++p->head)
            {
                p->value.integer *= 10;
                p->value.integer += p->data[p->head] - '0';
            }
            /* It is actually a float */
            if (p->head != p->end && p->data[p->head] == '.')
            {
                double fraction = 1.0;
                p->value.floating = (double)p->value.integer;
                for (p->head++; p->head != p->end && isdigit(p->data[p->head]);
                     ++p->head)
                {
                    fraction *= 0.1;
                    p->value.floating +=
                        fraction * (double)(p->data[p->head] - '0');
                }
                if (p->head != p->end && p->data[p->head] == 'f')
                    ++p->head;

                if (is_neg)
                    p->value.floating = -p->value.floating;
                return TOK_FLOAT;
            }

            if (is_neg)
                p->value.integer = -p->value.integer;
            return TOK_INTEGER;
        }

        /* String literal ".*?" (spans over newlines)*/
        if (p->data[p->head] == '"')
        {
            p->value.str.source = p->data;
            p->value.str.off = ++p->head;
            for (; p->head != p->end; ++p->head)
                if (p->data[p->head] == '"' && p->data[p->head - 1] != '\\')
                    break;
            if (p->head == p->end)
                return parser_error(p, "Missing closing quote on string\n");
            p->value.str.len = p->head++ - p->value.str.off;
            return TOK_STRING;
        }

        /* Identifier [a-zA-Z_-][a-zA-Z0-9_-]* */
        if (isalpha(p->data[p->head]) || p->data[p->head] == '_' ||
            p->data[p->head] == '-')
        {
            p->value.str.source = p->data;
            p->value.str.off = p->head++;
            while (p->head != p->end &&
                   (isalnum(p->data[p->head]) || p->data[p->head] == '_' ||
                    p->data[p->head] == '-'))
            {
                p->head++;
            }
            p->value.str.len = p->head - p->value.str.off;
            return TOK_IDENTIFIER;
        }

        p->tail = ++p->head;
    }

    return TOK_END;
}

enum token scan_until_section(struct parser* p)
{
    enum token tok;
    while (1)
    {
        tok = scan_next(p);
        if (tok == TOK_END || tok == TOK_ERROR)
            return tok;
        if (tok == TOK_IDENTIFIER && cstr_equal("SECTION", p->value.str))
            return tok;
    }
}

struct ll
{
    struct ll* next;
};

static void ll_append(struct ll** head, struct ll* node)
{
    while (*head)
        head = &(*head)->next;
    *head = node;
}

static void ll_remove(struct ll** node)
{
    *node = (*node)->next;
}

static void ll_reverse(struct ll** head)
{
    struct ll* next;
    struct ll* prev = NULL;
    while (*head)
    {
        next = (*head)->next;
        (*head)->next = prev;
        prev = *head;
        *head = next;
    }
    *head = prev;
}

enum c_data_type
{
    CDT_UNKNOWN,

    CDT_STR_FIXED,
    CDT_STR_DYNAMIC,
    CDT_STR_CUSTOM,

    CDT_STRLIST_FIXED,
    CDT_STRLIST_DYNAMIC,
    CDT_STRLIST_CUSTOM,

    CDT_BOOL,

    CDT_I8,
    CDT_U8,
    CDT_I16,
    CDT_U16,
    CDT_I32,
    CDT_U32,

    CDT_FLOAT,

    CDT_BITFIELD = (1 << 4)
};

#define cdt_switch(type) switch (type & ~CDT_BITFIELD)

enum value_type
{
    VT_INTEGER,
    VT_FLOAT,
    VT_STRING,
    VT_STRLIST
};

struct strlist
{
    struct strlist* next;
    struct strview  str;
};

struct value
{
    union
    {
        int64_t         integer;
        double          floating;
        struct strview  str;
        struct strlist* strlist;
    } value;
    enum value_type type;
};

struct attributes
{
    struct value   default_value;
    struct value   min, max;
    struct strview str_api_prefix;
    struct strview strlist_api_prefix;
};

struct key
{
    struct key*       next;
    struct strview    name;
    struct attributes attr;
    enum c_data_type  type;
};

struct section
{
    struct section* next;
    struct strview  name;
    struct strview  struct_name;
    struct strview  struct_def;
    struct key*     keys;
};

struct root
{
    struct section* sections;
};

static struct strlist* strlist_create(struct strview str)
{
    struct strlist* list = malloc(sizeof *list);
    list->next = NULL;
    list->str = str;
    return list;
}

static struct section* section_create(
    struct root* root, struct strview name, struct strview struct_name)
{
    struct section* section = malloc(sizeof *section);
    section->next = NULL;
    section->name = name;
    section->struct_name = struct_name;
    section->struct_def = empty_strview();
    section->keys = NULL;
    ll_append((struct ll**)&root->sections, (struct ll*)section);
    return section;
}

static struct key*
key_create(struct section* section, struct strview name, enum c_data_type type)
{
    struct key* key = malloc(sizeof *key);
    key->next = NULL;
    key->name = name;
    key->type = type;
    memset(&key->attr, 0, sizeof(key->attr));
    ll_append((struct ll**)&section->keys, (struct ll*)key);
    return key;
}

static void
attributes_set_default_for_type(struct attributes* attr, enum c_data_type type)
{
    cdt_switch(type)
    {
        case CDT_UNKNOWN: break;
        case CDT_STR_FIXED:
            attr->default_value.type = VT_STRING;
            attr->default_value.value.str = empty_strview();
            attr->str_api_prefix = cstr_strview("c_str_fixed");
            break;
        case CDT_STR_DYNAMIC:
            attr->default_value.type = VT_STRING;
            attr->default_value.value.str = empty_strview();
            attr->str_api_prefix = cstr_strview("c_str_dyn");
            break;
        case CDT_STR_CUSTOM:
            attr->default_value.type = VT_STRING;
            attr->default_value.value.str = empty_strview();
            /* str_api gets set by the STRING() attribute */
            break;
        case CDT_STRLIST_FIXED:
            attr->default_value.type = VT_STRLIST;
            attr->default_value.value.strlist = NULL;
            attr->strlist_api_prefix = cstr_strview("c_strlist_fixed");
            break;
        case CDT_STRLIST_DYNAMIC:
            attr->default_value.type = VT_STRLIST;
            attr->default_value.value.strlist = NULL;
            attr->strlist_api_prefix = cstr_strview("c_strlist_dyn");
            break;
        case CDT_STRLIST_CUSTOM:
            attr->default_value.type = VT_STRLIST;
            attr->default_value.value.strlist = NULL;
            /* str_api gets set by the STRINGLIST() attribute */
            break;
        case CDT_BOOL:
        case CDT_I8:
        case CDT_U8:
        case CDT_I16:
        case CDT_U16:
        case CDT_I32:
        case CDT_U32:
            attr->default_value.type = VT_INTEGER;
            attr->min.type = VT_INTEGER;
            attr->max.type = VT_INTEGER;
            attr->default_value.value.integer = 0;
            break;
        case CDT_FLOAT:
            attr->default_value.type = VT_FLOAT;
            attr->default_value.value.floating = 0.0;
            break;
        case CDT_BITFIELD: break;
    }

#define SET_MIN_MAX(attr, min_value, max_value)                                \
    do                                                                         \
    {                                                                          \
        attr->min.value.integer = min_value;                                   \
        attr->max.value.integer = max_value;                                   \
    } while (0)
    cdt_switch(type)
    {
        case CDT_UNKNOWN: break;
        case CDT_STR_FIXED: break;
        case CDT_STR_DYNAMIC: break;
        case CDT_STR_CUSTOM: break;
        case CDT_STRLIST_FIXED: break;
        case CDT_STRLIST_DYNAMIC: break;
        case CDT_STRLIST_CUSTOM: break;
        case CDT_BOOL: SET_MIN_MAX(attr, 0, 1); break;
        case CDT_I8: SET_MIN_MAX(attr, INT8_MIN, INT8_MAX); break;
        case CDT_U8: SET_MIN_MAX(attr, 0, UINT8_MAX); break;
        case CDT_I16: SET_MIN_MAX(attr, INT16_MIN, INT16_MAX); break;
        case CDT_U16: SET_MIN_MAX(attr, 0, UINT16_MAX); break;
        case CDT_I32: SET_MIN_MAX(attr, INT32_MIN, INT32_MAX); break;
        case CDT_U32: SET_MIN_MAX(attr, 0, UINT32_MAX); break;
        case CDT_FLOAT:
            attr->min.value.floating = -DBL_MAX;
            attr->max.value.floating = DBL_MAX;
            break;
        case CDT_BITFIELD: break;
    }
#undef X

    if (type & CDT_BITFIELD)
        SET_MIN_MAX(attr, 0, 1);
}

static enum token
parse_basic_data_type(struct parser* p, enum token tok, enum c_data_type* type)
{
    int            is_unsigned, is_basic_type;
    struct strview name;

    is_unsigned = 0;
    if (cstr_equal("unsigned", p->value.str))
    {
        is_unsigned = 1;
        tok = scan_next(p);
        if (tok != TOK_IDENTIFIER)
            return parser_error(p, "Expected data type after 'unsigned'\n");
    }

    is_basic_type = 1;
    name = p->value.str;
    if (cstr_equal("bool", name))
        *type = CDT_BOOL;
    else if (cstr_equal("char", name))
        *type = CDT_I8;
    else if (cstr_equal("u8", name) || cstr_equal("uint8_t", name))
        *type = CDT_U8;
    else if (cstr_equal("i8", name) || cstr_equal("int8_t", name))
        *type = CDT_I8;
    else if (cstr_equal("u16", name) || cstr_equal("uint16_t", name))
        *type = CDT_U16;
    else if (cstr_equal("i16", name) || cstr_equal("int16_t", name))
        *type = CDT_I16;
    else if (cstr_equal("u32", name) || cstr_equal("uint32_t", name))
        *type = CDT_U32;
    else if (cstr_equal("i32", name) || cstr_equal("int32_t", name))
        *type = CDT_I32;
    else if (cstr_equal("int", name))
        *type = CDT_I32;
    else if (cstr_equal("float", name) || cstr_equal("double", name))
        *type = CDT_FLOAT;
    else
        is_basic_type = 0;

    if (is_basic_type)
    {
        tok = scan_next(p);
        if (is_unsigned)
            switch (*type)
            {
                case CDT_I8: *type = CDT_U8; break;
                case CDT_I16: *type = CDT_U16; break;
                case CDT_I32: *type = CDT_U32; break;
                default:
                    return parser_error(
                        p,
                        "Unsigned modifier not allowed for type %.*s\n",
                        p->value.str.len,
                        p->value.str.source + p->value.str.off);
            }

        if (*type == CDT_I8 && tok == '*')
        {
            *type = CDT_STR_DYNAMIC;
            tok = scan_next(p);
            if (tok == '*')
            {
                *type = CDT_STRLIST_DYNAMIC;
                tok = scan_next(p);
            }
        }

        return tok;
    }

    if (is_unsigned)
    {
        *type = CDT_U32;
        return tok;
    }

    /* NOTE: The assumption is that this function returns with a token pointing
     * to the identifier of the field name in the struct, so the key_name can
     * easily be set. This means if we encounter a non-basic type, we need to
     * parse that type anyway.
     *
     * Because C can get crazy, I just hard-coded the most common cases here. */
    if (cstr_equal("struct", p->value.str))
    {
        scan_next(p); /* name of struct type */
        do
            tok = scan_next(p);
        while (tok != TOK_IDENTIFIER);
    }

    *type = CDT_UNKNOWN;
    return tok;
}

static int enforce_constrain_range(
    const struct parser* p, enum token tok, enum c_data_type type)
{
#define CHECK_INT_RANGE(min, max)                                              \
    if (p->value.integer < min || p->value.integer > max)                      \
        return parser_error(                                                   \
            p, "Value in CONSTRAIN() must be in range %d to %d\n", min, max);
    cdt_switch(type)
    {
        case CDT_UNKNOWN: return -1;
        case CDT_STR_FIXED: break;
        case CDT_STR_DYNAMIC: break;
        case CDT_STR_CUSTOM: break;
        case CDT_STRLIST_FIXED: break;
        case CDT_STRLIST_DYNAMIC: break;
        case CDT_STRLIST_CUSTOM: break;
        case CDT_BOOL:
            if (tok == TOK_INTEGER)
                CHECK_INT_RANGE(0, 1);
            break;
        case CDT_I8: CHECK_INT_RANGE(INT8_MIN, INT8_MAX); break;
        case CDT_U8: CHECK_INT_RANGE(0, UINT8_MAX); break;
        case CDT_I16: CHECK_INT_RANGE(INT16_MIN, INT16_MAX); break;
        case CDT_U16: CHECK_INT_RANGE(0, UINT16_MAX); break;
        case CDT_I32: CHECK_INT_RANGE(INT32_MIN, INT32_MAX); break;
        case CDT_U32: CHECK_INT_RANGE(0, UINT32_MAX); break;
        case CDT_FLOAT: break;
        case CDT_BITFIELD: break;
    }
#undef CHECK_INT_RANGE
    return 0;
}

static enum token parse_attribute_default(
    struct parser* p, enum c_data_type type, struct attributes* attr)
{
    enum token tok;

    if (scan_next(p) != '(')
        return parser_error(p, "Expected '(' after 'DEFAULT'\n");

    tok = scan_next(p);
    cdt_switch(type)
    {
        case CDT_UNKNOWN: return -1;
        case CDT_STR_FIXED:
        case CDT_STR_DYNAMIC:
        case CDT_STR_CUSTOM:
            if (tok != TOK_STRING)
                return parser_error(
                    p,
                    "Type in DEFAULT() does not match declared type in "
                    "struct. Expected a string.\n");
            attr->default_value.type = VT_STRING;
            attr->default_value.value.str = p->value.str;
            break;
        case CDT_STRLIST_FIXED:
        case CDT_STRLIST_DYNAMIC:
        case CDT_STRLIST_CUSTOM:
            if (tok != TOK_STRING)
                return parser_error(
                    p,
                    "Type in DEFAULT() does not match declared type in "
                    "struct. Expected a string.\n");
            attr->default_value.type = VT_STRLIST;
            ll_append(
                (struct ll**)&attr->default_value.value.strlist,
                (struct ll*)strlist_create(p->value.str));
            break;
        case CDT_BOOL:
            attr->default_value.type = VT_INTEGER;
            if (tok == TOK_INTEGER)
            {
                if (p->value.integer != 0 && p->value.integer != 1)
                    return parser_error(
                        p,
                        "Boolean value in DEFAULT() must be either 0 or "
                        "1, not %d\n",
                        p->value.integer);
                attr->default_value.value.integer = p->value.integer;
            }
            else if (tok == TOK_IDENTIFIER)
            {
                if (cstr_equal("true", p->value.str))
                    attr->default_value.value.integer = 1;
                else if (cstr_equal("false", p->value.str))
                    attr->default_value.value.integer = 0;
                else
                    return parser_error(
                        p,
                        "Boolean value in DEFAULT() must be either 'true' or "
                        "'false', not \"%.*s\"\n",
                        p->value.str.len,
                        p->value.str.source + p->value.str.off);
            }
            else
                return parser_error(
                    p,
                    "Type in DEFAULT() does not match declared type in "
                    "struct. Expected a boolean.\n");
            break;
        case CDT_I8:
        case CDT_U8:
        case CDT_I16:
        case CDT_U16:
        case CDT_I32:
        case CDT_U32:
            if (tok != TOK_INTEGER)
                return parser_error(
                    p,
                    "Type in DEFAULT() does not match declared type in "
                    "struct. Expected an integer.\n");
            attr->default_value.type = VT_INTEGER;
            attr->default_value.value.integer = p->value.integer;
            break;
        case CDT_FLOAT:
            if (tok != TOK_FLOAT && tok != TOK_INTEGER)
                return parser_error(
                    p,
                    "Type in DEFAULT() does not match declared type in struct. "
                    "Expected a float.\n");
            attr->default_value.type = VT_FLOAT;
            if (tok == TOK_FLOAT)
                attr->default_value.value.floating = p->value.floating;
            else
                attr->default_value.value.floating = (double)p->value.integer;
            break;
        case CDT_BITFIELD: break;
    }

    if (enforce_constrain_range(p, tok, type) != 0)
        return -1;

    if (scan_next(p) != ')')
        return parser_error(p, "Missing closing ')' after 'DEFAULT'\n");

    return scan_next(p);
}

static int enforce_constrain_type(
    const struct parser* p, enum token tok, enum c_data_type type)
{
    cdt_switch(type)
    {
        case CDT_UNKNOWN: return -1;
        case CDT_STR_FIXED:
        case CDT_STR_DYNAMIC:
        case CDT_STR_CUSTOM:
            return parser_error(
                p, "CONSTRAIN() doesn't make sense for string types\n");
        case CDT_STRLIST_FIXED:
        case CDT_STRLIST_DYNAMIC:
        case CDT_STRLIST_CUSTOM:
            return parser_error(
                p, "CONSTRAIN() doesn't make sense for string-list types\n");
        case CDT_BOOL:
            return parser_error(
                p, "CONSTRAIN() doesn't make sense for boolean types\n");
        case CDT_I8:
        case CDT_U8:
        case CDT_I16:
        case CDT_U16:
        case CDT_I32:
        case CDT_U32:
            if (tok != TOK_INTEGER)
                return parser_error(
                    p,
                    "Type in CONSTRAIN() does not match declared type in "
                    "struct. Expected an integer.\n");
            break;
        case CDT_FLOAT:
            if (tok != TOK_FLOAT && tok != TOK_INTEGER)
                return parser_error(
                    p,
                    "Type in CONSTRAIN() does not match declared type in "
                    "struct. Expected a float.\n");
            break;
        case CDT_BITFIELD: break;
    }

    return 0;
}

static enum token parse_attribute_constrain(
    struct parser* p, enum c_data_type type, struct attributes* attr)
{
    enum token tok;

    if (scan_next(p) != '(')
        return parser_error(p, "Expected '(' after 'CONSTRAIN'\n");

    tok = scan_next(p);
    if (enforce_constrain_type(p, tok, type) != 0)
        return -1;
    if (enforce_constrain_range(p, tok, type) != 0)
        return -1;
    attr->min.type = VT_INTEGER;
    attr->min.value.integer = p->value.integer;

    if (scan_next(p) != ',')
        return parser_error(p, "Missing ',' after value in CONSTRAIN\n");

    tok = scan_next(p);
    if (enforce_constrain_type(p, tok, type) != 0)
        return -1;
    if (enforce_constrain_range(p, tok, type) != 0)
        return -1;
    attr->max.type = VT_INTEGER;
    attr->max.value.integer = p->value.integer;

    if (attr->max.value.integer < attr->min.value.integer)
        return parser_error(
            p,
            "Maximum value in CONSTRAIN() must be greater than or equal to "
            "minimum value\n");

    if (scan_next(p) != ')')
        return parser_error(p, "Missing closing ')' after 'CONSTRAIN'\n");

    return scan_next(p);
}

static enum token parse_attribute_ignore(struct parser* p, struct key** key)
{
    if (scan_next(p) != '(')
        return parser_error(p, "Expected '(' after 'CONSTRAIN'\n");
    if (scan_next(p) != ')')
        return parser_error(p, "Missing closing ')' after 'CONSTRAIN'\n");

    ll_remove((struct ll**)key);
    return scan_next(p);
}

static enum token parse_custom_string(struct parser* p, struct attributes* attr)
{
    if (scan_next(p) != '(')
        return parser_error(p, "Expected '(' after 'STRING'\n");

    if (scan_next(p) != TOK_IDENTIFIER)
        return parser_error(p, "Missing API prefix in 'STRING()'\n");
    attr->str_api_prefix = p->value.str;

    if (scan_next(p) != ')')
        return parser_error(p, "Missing closing ')' for 'STRING()'\n");

    return scan_next(p);
}

static enum token
parse_custom_strlist(struct parser* p, struct attributes* attr)
{
    if (scan_next(p) != '(')
        return parser_error(p, "Expected '(' after 'STRINGLIST'\n");

    if (scan_next(p) != TOK_IDENTIFIER)
        return parser_error(p, "Missing API prefix in 'STRINGLIST()'\n");
    attr->strlist_api_prefix = p->value.str;

    if (scan_next(p) != ')')
        return parser_error(p, "Missing closing ')' for 'STRINGLIST()'\n");

    return scan_next(p);
}

static enum token parse_attributes(
    struct parser* p, enum token tok, enum c_data_type type, struct key** key)
{
    while (1)
    {
        if (tok != TOK_IDENTIFIER)
            return tok;

        if (cstr_equal("DEFAULT", p->value.str))
            tok = parse_attribute_default(p, type, &(*key)->attr);
        else if (cstr_equal("CONSTRAIN", p->value.str))
            tok = parse_attribute_constrain(p, type, &(*key)->attr);
        else if (cstr_equal("IGNORE", p->value.str))
            tok = parse_attribute_ignore(p, key);
        else if (cstr_equal("STRING", p->value.str))
            tok = parse_custom_string(p, &(*key)->attr);
        else if (cstr_equal("STRINGLIST", p->value.str))
            tok = parse_custom_strlist(p, &(*key)->attr);
        else
            return parser_error(
                p,
                "Unknown attribute \"%.*s\"\n",
                p->value.str.len,
                p->value.str.source + p->value.str.off);
    }
}

static enum token parse_struct_known_data_type(
    struct parser*   p,
    struct section*  section,
    enum c_data_type c_type,
    enum token       tok)
{
    struct strview key_name;
    struct key*    key;

    if (tok != TOK_IDENTIFIER)
        return parser_error(p, "Expected an identifier\n");
    key_name = p->value.str;

    tok = scan_next(p);
    if (tok == '[')
    {
        if (scan_next(p) != TOK_INTEGER)
            return parser_error(
                p, "Expected integer after '[' in struct definition\n");
        if (c_type == CDT_I8)
            c_type = CDT_STR_FIXED;
        else
            return parser_error(
                p, "Arrays are not supported for this data type.\n");
        if (scan_next(p) != ']')
            return parser_error(p, "Missing closing ']' in struct\n");
        tok = scan_next(p);

        if (tok == '[')
        {
            if (scan_next(p) != TOK_INTEGER)
                return parser_error(
                    p, "Expected integer after '[' in struct definition\n");
            c_type = CDT_STRLIST_FIXED;
            if (scan_next(p) != ']')
                return parser_error(p, "Missing closing ']' in struct\n");
            tok = scan_next(p);
        }
    }
    else if (tok == ':')
    {
        if (scan_next(p) != TOK_INTEGER)
            return parser_error(p, "Expected integer after ':'\n");
        switch (c_type)
        {
            case CDT_I8:
            case CDT_I16:
            case CDT_I32:
                if (p->value.integer <= 8)
                    c_type = CDT_I8 | CDT_BITFIELD;
                if (p->value.integer <= 16)
                    c_type = CDT_I16 | CDT_BITFIELD;
                if (p->value.integer <= 32)
                    c_type = CDT_I32 | CDT_BITFIELD;
                break;
            case CDT_U8:
            case CDT_U16:
            case CDT_U32:
                if (p->value.integer <= 8)
                    c_type = CDT_U8 | CDT_BITFIELD;
                if (p->value.integer <= 16)
                    c_type = CDT_U16 | CDT_BITFIELD;
                if (p->value.integer <= 32)
                    c_type = CDT_U32 | CDT_BITFIELD;
                break;
            default:
                return parser_error(
                    p, "Bitfields are not supported for this data type.\n");
        }
        tok = scan_next(p);
    }

    key = key_create(section, key_name, c_type);
    attributes_set_default_for_type(&key->attr, c_type);

    return parse_attributes(p, tok, c_type, &key);
}

static enum token parse_struct_unknown_data_type(
    struct parser* p, struct section* section, enum token tok)
{
    struct key*    key;
    struct parser  error_state = *p;
    int            ignore_attr = 0;
    struct strview key_name = empty_strview();

    /* In case we encounter a STRING(), need to know the field name in the
     * struct to create the key. This probably doesn't work for all possible
     * C/C++ types */
    if (tok == TOK_IDENTIFIER)
        key_name = p->value.str;

    while (1)
    {
        if (tok == TOK_ERROR || tok == TOK_END)
            break;
        else if (tok == TOK_IDENTIFIER && cstr_equal("IGNORE", p->value.str))
        {
            if (scan_next(p) != '(')
                return parser_error(p, "Expected opening '(' after IGNORE\n");
            if (scan_next(p) != ')')
                return parser_error(
                    p, "Missing closing ')' for IGNORE attribute\n");
            ignore_attr = 1;
        }
        else if (tok == TOK_IDENTIFIER && cstr_equal("STRING", p->value.str))
        {
            if (key_name.len == 0)
                return parser_error(
                    p,
                    "Can't create key because we failed to parse the name of "
                    "the variable. May have to use a less complex type, or "
                    "submit a bug report.\n");
            key = key_create(section, key_name, CDT_STR_CUSTOM);
            attributes_set_default_for_type(&key->attr, CDT_STR_CUSTOM);
            return parse_attributes(p, tok, CDT_STR_CUSTOM, &key);
        }
        else if (
            tok == TOK_IDENTIFIER && cstr_equal("STRINGLIST", p->value.str))
        {
            if (key_name.len == 0)
                return parser_error(
                    p,
                    "Can't create key because we failed to parse the name of "
                    "the variable. May have to use a less complex type, or "
                    "submit a bug report.\n");
            key = key_create(section, key_name, CDT_STRLIST_CUSTOM);
            attributes_set_default_for_type(&key->attr, CDT_STRLIST_CUSTOM);
            return parse_attributes(p, tok, CDT_STRLIST_CUSTOM, &key);
        }
        else if (tok == ';' || tok == ',')
        {
            if (ignore_attr)
                return tok;
            break;
        }

        tok = scan_next(p);
    }

    return parser_error(
        &error_state,
        "Unsupported data type. You can add the IGNORE() attribute to ignore "
        "this field.\n");
}

static enum token parse_struct(struct parser* p, struct section* section)
{
    enum token       tok;
    enum c_data_type c_type;

    while (1)
    {
        tok = scan_next(p);
        if (tok != TOK_IDENTIFIER)
            return tok;
        tok = parse_basic_data_type(p, tok, &c_type);
        if (tok == TOK_ERROR || tok == TOK_END)
            return tok;

    next_in_list:
        if (c_type != CDT_UNKNOWN)
            tok = parse_struct_known_data_type(p, section, c_type, tok);
        else
            tok = parse_struct_unknown_data_type(p, section, tok);

        if (tok != ';' && tok != ',')
            return parser_error(p, "Missing ';'\n");
        if (tok == ',')
        {
            tok = scan_next(p);
            goto next_in_list;
        }
    }
}

static int
parse(struct parser* p, struct root* root, int input_is_a_source_file)
{
    enum token tok;
    while (1)
    {
        tok = scan_until_section(p);
        switch (tok)
        {
            case TOK_ERROR: return -1;
            case TOK_END: return 0;

            case TOK_IDENTIFIER: {
                struct section* section;
                struct strview  section_name, struct_name, struct_def;
                if (scan_next(p) != '(')
                    return parser_error(p, "Expected '(' after SECTION name\n");
                if (scan_next(p) != TOK_STRING)
                    return parser_error(
                        p,
                        "Expected section name. Example: SECTION(\"name\")\n");
                section_name = p->value.str;
                if (scan_next(p) != ')')
                    return parser_error(p, "Missing closing ')'\n");

                if (scan_next(p) != TOK_IDENTIFIER ||
                    !cstr_equal("struct", p->value.str))
                    return parser_error(
                        p, "Expected 'struct' after SECTION name\n");
                struct_def = p->value.str;
                if (scan_next(p) != TOK_IDENTIFIER)
                    return parser_error(p, "Missing struct name\n");
                struct_name = p->value.str;

                if (scan_next(p) != '{')
                    return parser_error(p, "Expected '{' after struct name\n");
                section = section_create(root, section_name, struct_name);
                tok = parse_struct(p, section);
                if (tok == TOK_ERROR)
                    return -1;
                if (tok != '}')
                    return parser_error(p, "Expected closing '}'\n");

                if (input_is_a_source_file)
                {
                    struct_def.len = p->head - struct_def.off;
                    section->struct_def = struct_def;
                }

                break;
            }

            default: return parser_error(p, "Unexpected token\n");
        }
    }
}

/* ----------------------------------------------------------------------------
 * Generate header in-memory
 * ------------------------------------------------------------------------- */

#if defined(_WIN32)
#    define NL "\r\n"
#else
#    define NL "\n"
#endif

static void gen_header(struct mstream* ms, const struct root* root)
{
    const struct section* section;
    mstream_cstr(ms, "#pragma once\n\n");

    mstream_cstr(ms, "#include \"c-ini.h\"\n");
    mstream_cstr(ms, "#include <stdio.h>\n");
    mstream_cstr(ms, "#include <stdint.h>\n\n");

    mstream_cstr(ms, "#if defined(__cplusplus)\n");
    mstream_cstr(ms, "extern \"C\" {\n");
    mstream_cstr(ms, "#endif\n\n");

    mstream_cstr(ms, "struct c_ini_parser;\n\n");

    for (section = root->sections; section; section = section->next)
    {
        mstream_fmt(ms, "struct %S;\n", section->struct_name);
        mstream_fmt(
            ms,
            "int %S_init(struct %S* s);\n",
            section->struct_name,
            section->struct_name);
        mstream_fmt(
            ms,
            "void %S_deinit(struct %S* s);\n",
            section->struct_name,
            section->struct_name);
        mstream_fmt(
            ms,
            "int %S_parse(struct %S* s, const char* filename, const char* "
            "data, "
            "int len);\n",
            section->struct_name,
            section->struct_name);
        mstream_fmt(
            ms,
            "int %S_parse_all(const char* filename, const char* data, int len, "
            "int (*on_section)(struct c_ini_parser* parser, void* user_ptr),"
            "void* user_ptr);\n",
            section->struct_name,
            section->struct_name);
        mstream_fmt(
            ms,
            "int %S_parse_section(struct %S* s, struct c_ini_parser* p);\n",
            section->struct_name,
            section->struct_name);
        mstream_fmt(
            ms,
            "int %S_fwrite(const struct %S* s, FILE* f);\n",
            section->struct_name,
            section->struct_name);
        mstream_fmt(
            ms,
            "int %S_for_each_value(struct %S* s, int (*on_value)(void* value, "
            "int type, void*), void* user_ptr);\n",
            section->struct_name,
            section->struct_name,
            section->struct_name);
        mstream_cstr(ms, "\n");
    }

    mstream_cstr(ms, "#if defined(__cplusplus)\n");
    mstream_cstr(ms, "}\n");
    mstream_cstr(ms, "#endif\n\n");
}

/* ----------------------------------------------------------------------------
 * Generate source in-memory
 * ------------------------------------------------------------------------- */

static void gen_source_includes(struct mstream* ms, const struct cfg* cfg)
{
    int i;

    for (i = 0; i != cfg->input_count; ++i)
        if (!file_is_source_file(cfg->input_fnames[i]))
            mstream_fmt(ms, "#include \"%s\"\n", cfg->input_fnames[i]);

    if (cfg->output_header != NULL)
        mstream_fmt(ms, "#include \"%s\"\n", cfg->output_header);

    for (i = 0; i != cfg->include_file_count; ++i)
        mstream_fmt(ms, "#include \"%s\"\n", cfg->include_files[i]);

    mstream_cstr(ms, "#include <stdlib.h>\n");
    mstream_cstr(ms, "#include <ctype.h>\n");
    mstream_cstr(ms, "#include <string.h>\n");
    mstream_cstr(ms, "#include <stdarg.h>\n");
    mstream_cstr(ms, "#include <stdio.h>\n\n");
    mstream_cstr(ms, "#include <stdbool.h>\n\n");
}

static void gen_source_ini_parser(struct mstream* ms)
{
    mstream_cstr(
        ms,
        "struct c_ini_strspan\n"
        "{\n"
        "    int off, len;\n"
        "};\n\n");
    mstream_cstr(
        ms,
        "static struct c_ini_strspan c_ini_strspan(int off, int len)\n"
        "{\n"
        "    struct c_ini_strspan sv;\n"
        "    sv.off = off;\n"
        "    sv.len = len;\n"
        "    return sv;\n"
        "}\n\n");
    mstream_cstr(
        ms,
        "static int cstr_equal(const char* s1, struct c_ini_strspan s2, const "
        "char* "
        "data)\n"
        "{\n"
        "    if ((int)strlen(s1) != s2.len)\n"
        "        return 0;\n"
        "    return memcmp(s1, data + s2.off, s2.len) == 0;\n"
        "}\n\n");
    mstream_cstr(ms, "static int disable_colors = 0;\n\n");
    mstream_cstr(
        ms,
        "static const char* emph_style(void)\n"
        "{\n"
        "    return disable_colors ? \"\" : \"\\033[1;37m\";\n"
        "}\n"
        "static const char* error_style(void)\n"
        "{\n"
        "    return disable_colors ? \"\" : \"\\033[1;31m\";\n"
        "}\n"
        "static const char* underline_style(void)\n"
        "{\n"
        "    return disable_colors ? \"\" : \"\\033[1;31m\";\n"
        "}\n"
        "static const char* reset_style(void)\n"
        "{\n"
        "    return disable_colors ? \"\" : \"\\033[0m\";\n"
        "}\n\n");
    mstream_cstr(
        ms,
        "static void print_vflc(\n"
        "    const char*          filename,\n"
        "    const char*          source,\n"
        "    struct c_ini_strspan loc,\n"
        "    const char*          fmt,\n"
        "    va_list              ap)\n"
        "{\n"
        "    int i;\n"
        "    int l1, c1;\n"
        "\n"
        "    l1 = 1, c1 = 1;\n"
        "    for (i = 0; i != loc.off; i++)\n"
        "    {\n"
        "        c1++;\n"
        "        if (source[i] == '\\n')\n"
        "            l1++, c1 = 1;\n"
        "    }\n\n");
    mstream_cstr(
        ms,
        "    fprintf(\n"
        "        stderr,\n"
        "        \"%s%s:%d:%d:%s \",\n"
        "        emph_style(),\n"
        "        filename,\n"
        "        l1,\n"
        "        c1,\n"
        "        reset_style());\n"
        "    fprintf(stderr, \"%serror:%s \", error_style(), "
        "reset_style());\n"
        "    vfprintf(stderr, fmt, ap);\n"
        "}\n\n");
    mstream_cstr(
        ms,
        "static int num_digits(int value)\n{\n"
        "    int digits = 0;\n"
        "    while (value)\n"
        "        digits++, value /= 10;\n"
        "    return digits ? digits : 1;\n"
        "}\n\n");
    mstream_cstr(
        ms,
        "static void print_excerpt(const char* source, struct c_ini_strspan "
        "loc)\n"
        "{\n"
        "    int                  i;\n"
        "    int                  l1, c1, l2, c2;\n"
        "    int                  indent, max_indent;\n"
        "    int                  gutter_indent;\n"
        "    int                  line;\n"
        "    struct c_ini_strspan block;\n\n");
    mstream_cstr(
        ms,
        "    /* Calculate line column as well as beginning of block. The goal "
        "is to make\n"
        "     * \"block\" point to the first character in the line that "
        "contains the\n"
        "     * location. */\n"
        "    l1 = 1, c1 = 1, block.off = 0;\n"
        "    for (i = 0; i != loc.off; i++)\n"
        "    {\n"
        "        c1++;\n"
        "        if (source[i] == '\\n')\n"
        "            l1++, c1 = 1, block.off = i + 1;\n"
        "    }\n\n");
    mstream_cstr(
        ms,
        "    /* Calculate line/column of where the location ends */\n"
        "    l2 = l1, c2 = c1;\n"
        "    for (i = 0; i != loc.len; i++)\n"
        "    {\n"
        "        c2++;\n"
        "        if (source[loc.off + i] == '\\n')\n"
        "            l2++, c2 = 1;\n"
        "    }\n"
        "\n"
        "    /* Find the end of the line for block */\n"
        "    block.len = loc.off - block.off + loc.len;\n"
        "    for (; source[loc.off + i]; block.len++, i++)\n"
        "        if (source[loc.off + i] == '\\n')\n"
        "            break;\n\n");
    mstream_cstr(
        ms,
        "    /* We also keep track of the minimum indentation. This is used to "
        "unindent\n"
        "     * the block of code as much as possible when printing out the "
        "excerpt. */\n"
        "    max_indent = 10000;\n"
        "    for (i = 0; i != block.len;)\n"
        "    {\n"
        "        indent = 0;\n"
        "        for (; i != block.len; ++i, ++indent)\n"
        "        {\n"
        "            if (source[block.off + i] != ' ' && source[block.off + i] "
        "!= '\\t')\n"
        "                break;\n"
        "        }\n");
    mstream_cstr(
        ms,
        "        if (max_indent > indent)\n"
        "            max_indent = indent;\n"
        "\n"
        "        while (i != block.len)\n"
        "            if (source[block.off + i++] == '\\n')\n"
        "                break;\n"
        "    }\n\n");
    mstream_cstr(
        ms,
        "    /* Unindent columns */\n"
        "    c1 -= max_indent;\n"
        "    c2 -= max_indent;\n"
        "\n"
        "    gutter_indent = num_digits(l2);\n"
        "    gutter_indent += 2; /* Padding on either side of the line number "
        "*/\n\n");
    mstream_cstr(
        ms,
        "    /* Print line number, gutter, and block of code */\n"
        "    line = l1;\n"
        "    for (i = 0; i != block.len;)\n"
        "    {\n"
        "        fprintf(stderr, \"%*d | \", gutter_indent - 1, line);\n"
        "\n"
        "        if (i >= loc.off - block.off && i <= loc.off - block.off + "
        "loc.len)\n"
        "            fprintf(stderr, \"%s\", underline_style());\n\n");
    mstream_cstr(
        ms,
        "        indent = 0;\n"
        "        while (i != block.len)\n"
        "        {\n"
        "            if (i == loc.off - block.off)\n"
        "                fprintf(stderr, \"%s\", underline_style());\n"
        "            if (i == loc.off - block.off + loc.len)\n"
        "                fprintf(stderr, \"%s\", reset_style());\n"
        "\n"
        "            if (indent++ >= max_indent)\n"
        "                putc(source[block.off + i], stderr);\n\n");
    mstream_cstr(
        ms,
        "            if (source[block.off + i++] == '\\n')\n"
        "            {\n"
        "                if (i >= loc.off - block.off &&\n"
        "                    i <= loc.off - block.off + loc.len)\n"
        "                    fprintf(stderr, \"%s\", reset_style());\n"
        "                break;\n"
        "            }\n"
        "        }\n"
        "        line++;\n"
        "    }\n"
        "    fprintf(stderr, \"%s\\n\", reset_style());\n");
    mstream_cstr(
        ms,
        "    /* print underline */\n"
        "    if (c2 > c1)\n"
        "    {\n"
        "        fprintf(stderr, \"%*s|%*s\", gutter_indent, \"\", c1, \"\");\n"
        "        fprintf(stderr, \"%s\", underline_style());\n"
        "        putc('^', stderr);\n"
        "        for (i = c1 + 1; i < c2; ++i)\n"
        "            putc('~', stderr);\n"
        "        fprintf(stderr, \"%s\", reset_style());\n"
        "    }\n"
        "    else\n\n");
    mstream_cstr(
        ms,
        "    {\n"
        "        int col, max_col;\n"
        "\n"
        "        fprintf(stderr, \"%*s| \", gutter_indent, \"\");\n"
        "        fprintf(stderr, \"%s\", underline_style());\n"
        "        for (i = 1; i < c2; ++i)\n"
        "            putc('~', stderr);\n"
        "        for (; i < c1; ++i)\n"
        "            putc(' ', stderr);\n"
        "        putc('^', stderr);\n\n");
    mstream_cstr(
        ms,
        "        /* Have to find length of the longest line */\n"
        "        col = 1, max_col = 1;\n"
        "        for (i = 0; i != block.len; ++i)\n"
        "        {\n"
        "            if (max_col < col)\n"
        "                max_col = col;\n"
        "            col++;\n"
        "            if (source[block.off + i] == '\\n')\n"
        "                col = 1;\n"
        "        }\n"
        "        max_col -= max_indent;\n"
        "\n"
        "        for (i = c1 + 1; i < max_col; ++i)\n"
        "            putc('~', stderr);\n"
        "        fprintf(stderr, \"%s\", reset_style());\n"
        "    }\n"
        "\n"
        "    putc('\\n', stderr);\n"
        "}\n\n");
    mstream_cstr(
        ms,
        "enum token\n"
        "{\n"
        "    TOK_ERROR = -1,\n"
        "    TOK_END = 0,\n"
        "    TOK_LBRACKET = '[',\n"
        "    TOK_RBRACKET = ']',\n"
        "    TOK_EQUALS = '=',\n"
        "    TOK_COMMA = ',',\n"
        "    TOK_INTEGER = 256,\n"
        "    TOK_FLOAT,\n"
        "    TOK_STRING,\n"
        "    TOK_KEY\n"
        "};\n\n"
        "struct c_ini_parser\n"
        "{\n"
        "    const char* filename;\n"
        "    const char* source;\n"
        "    int         head, tail, end;\n"
        "    union\n"
        "    {\n"
        "        struct c_ini_strspan string;\n"
        "        double               float_literal;\n"
        "        int64_t              integer_literal;\n"
        "    } value;\n"
        "};\n\n");
    mstream_cstr(
        ms,
        "static void\n"
        "parser_init(struct c_ini_parser* p, const char* filename, const char* "
        "data, int len)\n"
        "{\n"
        "    p->filename = filename;\n"
        "    p->source = data;\n"
        "    p->end = len;\n"
        "    p->head = 0;\n"
        "    p->tail = 0;\n"
        "}\n\n");
    mstream_cstr(
        ms,
        "static int parser_error(struct c_ini_parser* p, const char* fmt, "
        "...)\n"
        "{\n"
        "    va_list              ap;\n"
        "    struct c_ini_strspan loc;\n"
        "    loc.off = p->tail;\n"
        "    loc.len = p->head - p->tail;\n"
        "    va_start(ap, fmt);\n"
        "    print_vflc(p->filename, p->source, loc, fmt, ap);\n"
        "    va_end(ap);\n"
        "    print_excerpt(p->source, loc);\n"
        "    return -1;\n"
        "}\n\n");
    mstream_cstr(
        ms,
        "static enum token scan_next(struct c_ini_parser* p)\n"
        "{\n"
        "    p->tail = p->head;\n"
        "    while (p->head != p->end)\n"
        "    {\n"
        "        /* Skip comments */\n"
        "        if (p->source[p->head] == '#' || p->source[p->head] == ';')\n"
        "        {\n"
        "            for (p->head++; p->head != p->end; p->head++)\n"
        "                if (p->source[p->head] == '\\n')\n"
        "                {\n"
        "                    p->head++;\n"
        "                    break;\n"
        "                }\n"
        "            p->tail = p->head;\n"
        "            continue;\n"
        "        }\n\n");
    mstream_cstr(
        ms,
        "        /* Special characters */\n"
        "        if (p->source[p->head] == '[')\n"
        "            return p->source[p->head++];\n"
        "        if (p->source[p->head] == ']')\n"
        "            return p->source[p->head++];\n"
        "        if (p->source[p->head] == '=')\n"
        "            return p->source[p->head++];\n"
        "        if (p->source[p->head] == ',')\n"
        "            return p->source[p->head++];\n\n");
    mstream_cstr(
        ms,
        "        /* Boolean */\n"
        "        if (p->end - p->head >= (int)sizeof(\"true\") - 1 &&\n"
        "            memcmp(p->source + p->head, \"true\", sizeof(\"true\") - "
        "1) == 0)\n"
        "        {\n"
        "            p->head += sizeof(\"true\") - 1;\n"
        "            p->value.integer_literal = 1;\n"
        "            return TOK_INTEGER;\n"
        "        }\n");
    mstream_cstr(
        ms,
        "        if (p->end - p->head >= (int)sizeof(\"false\") - 1 &&\n"
        "            memcmp(p->source + p->head, \"false\", sizeof(\"false\") "
        "- "
        "1) == 0)\n"
        "        {\n"
        "            p->head += sizeof(\"false\") - 1;\n"
        "            p->value.integer_literal = 0;\n"
        "            return TOK_INTEGER;\n"
        "        }\n\n");
    mstream_cstr(
        ms,
        "        /* Number */\n"
        "        if (isdigit(p->source[p->head]) || p->source[p->head] == "
        "'-')\n"
        "        {\n"
        "            char is_neg = p->source[p->head] == '-';\n"
        "            if (p->source[p->head] == '-')\n"
        "                p->head++;\n\n");
    mstream_cstr(
        ms,
        "            p->value.integer_literal = 0;\n"
        "            for (; p->head != p->end && isdigit(p->source[p->head]); "
        "++p->head)\n"
        "            {\n"
        "                p->value.integer_literal *= 10;\n"
        "                p->value.integer_literal += p->source[p->head] - "
        "'0';\n"
        "            }\n");
    mstream_cstr(
        ms,
        "            /* It is actually a float */\n"
        "            if (p->head != p->end && p->source[p->head] == '.')\n"
        "            {\n"
        "                double fraction = 1.0;\n"
        "                p->value.float_literal = "
        "(double)p->value.integer_literal;\n"
        "                for (p->head++; p->head != p->end && "
        "isdigit(p->source[p->head]);\n"
        "                     ++p->head)\n");
    mstream_cstr(
        ms,
        "                {\n"
        "                    fraction *= 0.1;\n"
        "                    p->value.float_literal +=\n"
        "                        fraction * (double)(p->source[p->head] - "
        "'0');\n"
        "                }\n"
        "                if (p->head != p->end && p->source[p->head] == 'f')\n"
        "                    ++p->head;\n\n");
    mstream_cstr(
        ms,
        "                if (is_neg)\n"
        "                    p->value.float_literal = "
        "-p->value.float_literal;\n"
        "                return TOK_FLOAT;\n"
        "            }\n"
        "\n"
        "            if (is_neg)\n"
        "                p->value.integer_literal = "
        "-p->value.integer_literal;\n"
        "            return TOK_INTEGER;\n"
        "        }\n");
    mstream_cstr(
        ms,
        "        /* String literal \".*?\" (spans over newlines)*/\n"
        "        if (p->source[p->head] == '\"')\n"
        "        {\n"
        "            int tail = ++p->head;\n"
        "            for (; p->head != p->end; ++p->head)\n"
        "                if (p->source[p->head] == '\"' && p->source[p->head - "
        "1] != '\\\\')\n");
    mstream_cstr(
        ms,
        "                    break;\n"
        "            if (p->head == p->end)\n"
        "                return parser_error(p, \"Missing closing quote on "
        "string\\n\");\n"
        "            p->value.string = c_ini_strspan(tail, p->head++ - tail);\n"
        "            return TOK_STRING;\n"
        "        }\n\n");
    mstream_cstr(
        ms,
        "        /* Key */\n"
        "        if (isalpha(p->source[p->head]))\n"
        "        {\n"
        "            while (p->head != p->end &&\n"
        "                   (isalnum(p->source[p->head]) || p->source[p->head] "
        "== '_'))\n"
        "            {\n"
        "                p->head++;\n"
        "            }\n"
        "            p->value.string = c_ini_strspan(p->tail, p->head - "
        "p->tail);\n"
        "            return TOK_KEY;\n"
        "        }\n\n");
    mstream_cstr(
        ms,
        "        /* Ignore everything else */\n"
        "        p->tail = ++p->head;\n"
        "    }\n"
        "\n"
        "    return TOK_END;\n"
        "}\n\n");
}

static void gen_source_c_str_dyn(struct mstream* ms)
{
    /* Built-in "custom string" functions for C-strings */
    mstream_cstr(
        ms,
        "static int c_str_dyn_init(char** s)\n{    \n"
        "    *s = malloc(1);\n"
        "    if (*s == NULL)\n"
        "        return -1;\n"
        "    (*s)[0] = '\\0';\n"
        "    return 0;\n"
        "}\n\n");
    mstream_cstr(
        ms,
        "static void c_str_dyn_deinit(char* s)\n{    \n"
        "    free(s);\n"
        "}\n\n");
    mstream_cstr(
        ms,
        "static int c_str_dyn_set(char** s, const char* data, int len)\n{    \n"
        "    char* ns = realloc(*s, len + 1);\n"
        "    if (ns == NULL)\n"
        "        return -1;\n"
        "    memcpy(ns, data, len);\n"
        "    ns[len] = '\\0';\n"
        "    *s = ns;\n"
        "    return 0;\n"
        "}\n\n");
    mstream_cstr(
        ms,
        "static const char* c_str_dyn_data(const char* s)\n{\n"
        "    return s;\n"
        "}\n\n");
    mstream_cstr(
        ms,
        "static int c_str_dyn_len(const char* s)\n{\n"
        "    return (int)strlen(s);\n"
        "}\n\n");
}

static void gen_source_c_strlist_dyn(struct mstream* ms)
{
    /* Built-in "custom stringlist" functions for C-strings */
    mstream_cstr(
        ms,
        "static int c_strlist_dyn_init(char*** l)\n{    \n"
        "    *l = malloc(sizeof(char**));\n"
        "    if (*l == NULL)\n"
        "        return -1;\n"
        "    (*l)[0] = NULL;\n"
        "    return 0;\n"
        "}\n\n");
    mstream_cstr(
        ms,
        "static void c_strlist_dyn_deinit(char** l)\n{    \n"
        "    char** p;\n"
        "    for (p = l; *p; ++p)\n"
        "        free(*p);\n"
        "    free(l);\n"
        "}\n\n");
    mstream_cstr(
        ms,
        "static int c_strlist_dyn_add(char*** l, const char* data, int "
        "len)\n{\n"
        "    char** p;\n"
        "    int list_len = 0;\n"
        "    for (p = *l; *p; ++p)\n"
        "        list_len++;\n"
        "    p = realloc(*l, sizeof(char**) * (list_len + 2));\n"
        "    if (p == NULL)\n"
        "        return -1;\n"
        "    *l = p;\n"
        "    p[list_len + 1] = NULL;\n"
        "    p[list_len] = malloc(len + 1);\n"
        "    if (p[list_len] == NULL)\n"
        "        return -1;\n"
        "    memcpy(p[list_len], data, len);\n"
        "    p[list_len][len] = '\\0';\n"
        "    return 0;\n"
        "}\n\n");
    mstream_cstr(
        ms,
        "static void c_strlist_dyn_clear(char** l)\n{\n"
        "    char** p;\n"
        "    for (p = l; *p; ++p)\n"
        "        free(*p);\n"
        "    l[0] = NULL;\n"
        "}\n\n");
}

static void gen_source_helpers(struct mstream* ms, const struct root* root)
{
    const struct section* section;
    const struct key*     key;

    /* May need to copy the entire struct definition into the source file, if
     * the struct was originally defined in a source file */
    for (section = root->sections; section; section = section->next)
    {
        if (section->struct_def.len == 0)
            continue;
        mstream_fmt(ms, "%S;\n\n", section->struct_def);
    }

    for (section = root->sections; section; section = section->next)
        for (key = section->keys; key; key = key->next)
            if (key->type == CDT_STR_DYNAMIC || key->type == CDT_STR_CUSTOM)
            {
                gen_source_c_str_dyn(ms);
                goto found_str_dynamic;
            }
found_str_dynamic:;

    for (section = root->sections; section; section = section->next)
        for (key = section->keys; key; key = key->next)
            if (key->type == CDT_STRLIST_DYNAMIC ||
                key->type == CDT_STRLIST_CUSTOM)
            {
                gen_source_c_strlist_dyn(ms);
                goto found_strlist_dynamic;
            }
found_strlist_dynamic:;
}

static void gen_source_init(struct mstream* ms, const struct section* section)
{
    const struct key*     key;
    const struct strlist* strlist;
    int                   i;

    mstream_fmt(
        ms,
        "int %S_init(struct %S* s)\n{\n",
        section->struct_name,
        section->struct_name);
    mstream_cstr(ms, "    memset(s, 0x00, sizeof *s);\n");
    for (key = section->keys; key; key = key->next)
    {
        cdt_switch(key->type)
        {
            case CDT_UNKNOWN: break;
            case CDT_STR_FIXED:
                mstream_fmt(
                    ms,
                    "    strcpy(s->%S, \"%S\");\n",
                    key->name,
                    key->attr.default_value.value.str);
                break;
            case CDT_STR_DYNAMIC:
            case CDT_STR_CUSTOM:
                mstream_fmt(
                    ms,
                    "    if (%S_init(&s->%S) != 0)\n"
                    "        goto %S_failed;\n",
                    key->attr.str_api_prefix,
                    key->name,
                    key->name);
                if (key->attr.default_value.value.str.len > 0)
                {
                    mstream_fmt(
                        ms,
                        "    if (%S_set(&s->%S, \"%S\", %d) != 0)\n"
                        "        goto %S_set_failed;\n",
                        key->attr.str_api_prefix,
                        key->name,
                        key->attr.default_value.value.str,
                        key->attr.default_value.value.str.len,
                        key->name);
                }
                break;
            case CDT_STRLIST_FIXED:
                strlist = key->attr.default_value.value.strlist;
                for (i = 0; strlist; strlist = strlist->next, i++)
                    mstream_fmt(
                        ms,
                        "    strcpy(s->%S[%d], \"%S\");\n",
                        key->name,
                        i,
                        strlist->str);
                break;
            case CDT_STRLIST_DYNAMIC:
            case CDT_STRLIST_CUSTOM:
                strlist = key->attr.default_value.value.strlist;
                mstream_fmt(
                    ms,
                    "    if (%S_init(&s->%S) != 0)\n"
                    "        goto %S_failed;\n",
                    key->attr.strlist_api_prefix,
                    key->name,
                    key->name);
                for (i = 0; strlist; strlist = strlist->next, i++)
                    mstream_fmt(
                        ms,
                        "    if (%S_add(&s->%S, \"%S\", (int)sizeof(\"%S\") - "
                        "1) != 0)\n"
                        "        goto %S_add_failed;\n",
                        key->attr.strlist_api_prefix,
                        key->name,
                        strlist->str,
                        strlist->str,
                        key->name);
                break;
            case CDT_BOOL:
            case CDT_I8:
            case CDT_U8:
            case CDT_I16:
            case CDT_U16:
            case CDT_I32:
            case CDT_U32:
                mstream_fmt(
                    ms,
                    "    s->%S = %d;\n",
                    key->name,
                    (int)key->attr.default_value.value.integer);
                break;
            case CDT_FLOAT:
                mstream_fmt(
                    ms,
                    "    s->%S = %f;\n",
                    key->name,
                    key->attr.default_value.value.floating);
                break;
            case CDT_BITFIELD: break;
        }
    }
    mstream_cstr(ms, "    return 0;\n\n    ");

    ll_reverse((struct ll**)&section->keys);
    for (key = section->keys; key; key = key->next)
    {
        cdt_switch(key->type)
        {
            case CDT_UNKNOWN:
            case CDT_STR_FIXED: break;
            case CDT_STR_DYNAMIC:
            case CDT_STR_CUSTOM:
                if (key->attr.default_value.value.str.len > 0)
                    mstream_fmt(ms, "%S_set_failed: ", key->name);
                mstream_fmt(
                    ms,
                    "%S_deinit(s->%S);\n    ",
                    key->attr.str_api_prefix,
                    key->name);
                mstream_fmt(ms, "%S_failed: ", key->name);
                break;
            case CDT_STRLIST_FIXED: break;
            case CDT_STRLIST_DYNAMIC:
            case CDT_STRLIST_CUSTOM:
                strlist = key->attr.default_value.value.strlist;
                if (strlist != NULL)
                    mstream_fmt(ms, "%S_add_failed: ", key->name);
                mstream_fmt(
                    ms,
                    "%S_deinit(s->%S);\n    ",
                    key->attr.strlist_api_prefix,
                    key->name);
                mstream_fmt(ms, "%S_failed: ", key->name);
                break;
            case CDT_BOOL:
            case CDT_I8:
            case CDT_U8:
            case CDT_I16:
            case CDT_U16:
            case CDT_I32:
            case CDT_U32:
            case CDT_FLOAT: break;
            case CDT_BITFIELD: break;
        }
    }
    ll_reverse((struct ll**)&section->keys);

    mstream_cstr(ms, "return -1;\n}\n\n");
}

static void gen_source_deinit(struct mstream* ms, const struct section* section)
{
    const struct key* key;
    mstream_fmt(
        ms,
        "void %S_deinit(struct %S* s)\n{\n",
        section->struct_name,
        section->struct_name);
    mstream_cstr(ms, "    (void)s;\n");
    for (key = section->keys; key; key = key->next)
    {
        cdt_switch(key->type)
        {
            case CDT_UNKNOWN: break;
            case CDT_STR_FIXED: break;
            case CDT_STR_DYNAMIC:
            case CDT_STR_CUSTOM:
                mstream_fmt(
                    ms,
                    "    %S_deinit(s->%S);\n",
                    key->attr.str_api_prefix,
                    key->name);
                break;
            case CDT_STRLIST_FIXED: break;
            case CDT_STRLIST_DYNAMIC:
            case CDT_STRLIST_CUSTOM:
                mstream_fmt(
                    ms,
                    "    %S_deinit(s->%S);\n",
                    key->attr.strlist_api_prefix,
                    key->name);
                break;
            case CDT_BOOL:
            case CDT_I8:
            case CDT_U8:
            case CDT_I16:
            case CDT_U16:
            case CDT_I32:
            case CDT_U32: break;
            case CDT_FLOAT: break;
            case CDT_BITFIELD: break;
        }
    }
    mstream_cstr(ms, "}\n\n");
}

static void gen_source_fwrite(struct mstream* ms, const struct section* section)
{
    const struct key* key;
    struct strview    api;

    mstream_fmt(
        ms,
        "int %S_fwrite(const struct %S* s, FILE* f)\n{\n",
        section->struct_name,
        section->struct_name);
    mstream_cstr(ms, "    char** list;\n");
    mstream_cstr(ms, "    int i;\n");
    mstream_cstr(ms, "    (void)list, (void)i;\n");
    mstream_fmt(ms, "    fprintf(f, \"[%S]\\n\");\n", section->name);
    for (key = section->keys; key; key = key->next)
    {
        cdt_switch(key->type)
        {
            case CDT_UNKNOWN: break;
            case CDT_STR_FIXED:
                mstream_fmt(
                    ms,
                    "    fprintf(f, \"%S = \\\"%%s\\\"\\n\", s->%S);\n",
                    key->name,
                    key->name);
                break;
            case CDT_STR_DYNAMIC:
            case CDT_STR_CUSTOM:
                mstream_fmt(
                    ms,
                    "    fprintf(f, \"%S = \\\"%%.*s\\\"\\n\", "
                    "%S_len(s->%S), %S_data(s->%S));\n",
                    key->name,
                    key->attr.str_api_prefix,
                    key->name,
                    key->attr.str_api_prefix,
                    key->name);
                break;
            case CDT_STRLIST_FIXED:
                mstream_fmt(ms, "    if (s->%S[0][0] != '\\0')\n", key->name);
                mstream_fmt(ms, "        fprintf(f, \"%S = \");\n", key->name);
                mstream_fmt(
                    ms,
                    "    for (i = 0; i < (int)sizeof(s->%S) / "
                    "(int)sizeof(*s->%S) && *s->%S[i]; ++i)\n"
                    "    {\n"
                    "        if (i != 0) fprintf(f, \", \");\n"
                    "        fprintf(f, \"\\\"%%s\\\"\", s->%S[i]);\n"
                    "    }\n",
                    key->name,
                    key->name,
                    key->name,
                    key->name);
                mstream_fmt(ms, "    if (s->%S[0][0] != '\\0')\n", key->name);
                mstream_fmt(ms, "        fprintf(f, \"\\n\");\n", key->name);
                break;
            case CDT_STRLIST_DYNAMIC:
            case CDT_STRLIST_CUSTOM:
                api = key->attr.strlist_api_prefix;
                mstream_fmt(
                    ms, "    if (%S_count(s->%S) > 0)\n", api, key->name);
                mstream_fmt(ms, "        fprintf(f, \"%S = \");\n", key->name);
                mstream_fmt(
                    ms,
                    "    for (i = 0; i != %S_count(s->%S); ++i)\n"
                    "    {\n"
                    "        if (i) fprintf(f, \", \");\n"
                    "        fprintf(f, \"\\\"%%s\\\"\", %S_cstr(s->%S, i));\n"
                    "    }\n",
                    api,
                    key->name,
                    api,
                    key->name);
                mstream_fmt(
                    ms, "    if (%S_count(s->%S) > 0)\n", api, key->name);
                mstream_cstr(ms, "        fprintf(f, \"\\n\");\n");
                break;
            case CDT_BOOL:
                mstream_fmt(
                    ms,
                    "    fprintf(f, \"%S = %%s\\n\", "
                    "s->%S ? \"true\" : \"false\");\n",
                    key->name,
                    key->name);
                break;
            case CDT_I8:
            case CDT_U8:
            case CDT_I16:
            case CDT_U16:
            case CDT_I32:
            case CDT_U32:
                mstream_fmt(
                    ms,
                    "    fprintf(f, \"%S = %%d\\n\", s->%S);\n",
                    key->name,
                    key->name);
                break;
            case CDT_FLOAT:
                mstream_fmt(
                    ms,
                    "    fprintf(f, \"%S = %%.9g\\n\", s->%S);\n",
                    key->name,
                    key->name);
                break;
            case CDT_BITFIELD: break;
        }
    }
    mstream_cstr(ms, "    fprintf(f, \"\\n\");\n");
    mstream_cstr(ms, "    return 0;\n}\n\n");
}

static void gen_source_parse_key(
    struct mstream* ms, const struct section* section, const struct key* key)
{
    struct strview api;
    mstream_fmt(
        ms,
        "static enum token parse_%S__%S(\n"
        "    struct c_ini_parser* p, struct %S* s)\n"
        "{\n",
        section->struct_name,
        key->name,
        section->struct_name);
    cdt_switch(key->type)
    {
        case CDT_UNKNOWN: break;
        case CDT_STR_FIXED:
            mstream_fmt(
                ms,
                "    struct c_ini_strspan value;\n"
                "    if (scan_next(p) != TOK_STRING)\n"
                "        return parser_error(p, \"Expected a string literal "
                "for %S\\n\");\n",
                key->name);
            mstream_fmt(
                ms,
                "    value = p->value.string;\n\n"
                "    if (value.len >= (int)sizeof(s->%S))\n"
                "        return parser_error(\n"
                "            p,\n"
                "            \"\\\"%S\\\" can't be longer than %%d "
                "characters\\n\",\n"
                "            (int)sizeof(s->%S) - 1);\n\n",
                key->name,
                key->name,
                key->name);
            mstream_fmt(
                ms,
                "    memcpy(s->%S, p->source + value.off, value.len);\n"
                "    s->%S[value.len] = '\\0';\n\n"
                "    return scan_next(p);\n",
                key->name,
                key->name);
            break;
        case CDT_STR_DYNAMIC:
        case CDT_STR_CUSTOM:
            mstream_fmt(
                ms,
                "    if (scan_next(p) != TOK_STRING)\n"
                "        return parser_error(p, \"Expected a string literal of "
                "%S\\n\");\n\n",
                key->name);
            mstream_fmt(
                ms,
                "    if (%S_set(&s->%S, p->source + p->value.string.off, "
                "p->value.string.len) != 0)\n"
                "        return TOK_ERROR;\n\n"
                "    return scan_next(p);\n",
                key->attr.str_api_prefix,
                key->name);
            break;
        case CDT_STRLIST_FIXED:
            mstream_fmt(
                ms,
                "    enum token tok;\n"
                "    int        i = 0;\n"
                "    while (1)\n"
                "    {\n"
                "        if (scan_next(p) != TOK_STRING)\n"
                "            return parser_error(p,"
                "\"Expected a string literal for %S\\n\");\n\n",
                key->name);
            mstream_fmt(
                ms,
                "        if (p->value.string.len > (int)sizeof(*s->%S))\n"
                "            return parser_error(\n"
                "                p,\n"
                "                \"String literal is too large. Max size is "
                "%%d bytes.\\n\",\n"
                "                sizeof(*s->%S));\n",
                key->name,
                key->name);
            mstream_fmt(
                ms,
                "        if (i > (int)sizeof(s->%S) / (int)sizeof(*s->%S))\n"
                "            return parser_error(\n"
                "                p, \"Too many strings in list. Max size is "
                "%%d strings.\\n\");\n\n",
                key->name,
                key->name);
            mstream_fmt(
                ms,
                "        memcpy(\n"
                "            s->%S[i],\n"
                "            p->source + p->value.string.off,\n"
                "            p->value.string.len);\n"
                "        s->%S[i++][p->value.string.len] = '\\0';\n",
                key->name,
                key->name);
            mstream_cstr(
                ms,
                "        tok = scan_next(p);\n"
                "        if (tok != ',')\n"
                "            break;\n"
                "    }\n\n");
            mstream_fmt(
                ms,
                "    while (i != (int)sizeof(s->%S) / (int)sizeof(*s->%S))\n"
                "        s->%S[i++][0] = '\\0';\n\n"
                "    return tok;\n",
                key->name,
                key->name,
                key->name);
            break;
        case CDT_STRLIST_DYNAMIC:
        case CDT_STRLIST_CUSTOM:
            api = key->attr.strlist_api_prefix;
            mstream_fmt(
                ms,
                "    enum token tok;\n"
                "    %S_clear(s->%S);\n",
                api,
                key->name);
            mstream_fmt(
                ms,
                "    while (1)\n"
                "    {\n"
                "        if (scan_next(p) != TOK_STRING)\n"
                "            return parser_error(p,"
                "\"Expected a string literal for %S\\n\");\n\n",
                key->name);
            mstream_fmt(
                ms,
                "        if (%S_add(&s->%S, p->source + p->value.string.off, "
                "p->value.string.len) != 0)\n"
                "            return -1;\n",
                api,
                key->name);
            mstream_cstr(
                ms,
                "        tok = scan_next(p);\n"
                "        if (tok != ',')\n"
                "            break;\n"
                "    }\n\n"
                "    return tok;\n");
            break;
        case CDT_BOOL:
        case CDT_I8:
        case CDT_U8:
        case CDT_I16:
        case CDT_U16:
        case CDT_I32:
        case CDT_U32:
            mstream_fmt(
                ms,
                "    if (scan_next(p) != TOK_INTEGER)\n"
                "        return parser_error(p, \"Expected an integer literal "
                "for %S\\n\");\n\n",
                key->name);

            if (key->type != CDT_U32 && key->type != CDT_I32)
                mstream_fmt(
                    ms,
                    "    if (p->value.integer_literal < %d || "
                    "p->value.integer_literal > %d)\n"
                    "        return parser_error(p, \"\\\"%S\\\" must be "
                    "%d to %d\\n\");\n\n",
                    key->attr.min.value.integer,
                    key->attr.max.value.integer,
                    key->name,
                    key->attr.min.value.integer,
                    key->attr.max.value.integer);
            mstream_fmt(
                ms, "    s->%S = p->value.integer_literal;\n", key->name);
            mstream_cstr(ms, "    return scan_next(p);\n");
            break;
        case CDT_FLOAT:
            mstream_fmt(
                ms,
                "    double value;\n"
                "    enum token tok = scan_next(p);\n"
                "    if (tok != TOK_FLOAT && tok != TOK_INTEGER)\n"
                "        return parser_error(\n"
                "            p, \"Expected a floating point literal for "
                "%S\\n\");\n\n",
                key->name);
            mstream_cstr(
                ms,
                "    if (tok == TOK_FLOAT)\n"
                "        value = p->value.float_literal;\n"
                "    else\n"
                "        value = (double)p->value.integer_literal;\n");
            mstream_fmt(
                ms,
                "    if (value < %f || value > %f)\n"
                "        return parser_error(p, \"\\\"%S\\\" must be %f to "
                "%f\\n\");\n",
                key->attr.min.value.floating,
                key->attr.max.value.floating,
                key->name,
                key->attr.min.value.floating,
                key->attr.max.value.floating);
            mstream_fmt(
                ms,
                "\n"
                "    s->%S = value;\n"
                "    return scan_next(p);\n",
                key->name);
            break;
        case CDT_BITFIELD: break;
    }
    mstream_cstr(ms, "}\n\n");
}

static void
gen_source_parse_section(struct mstream* ms, const struct section* section)
{
    struct key* key;
    for (key = section->keys; key; key = key->next)
        gen_source_parse_key(ms, section, key);

    mstream_fmt(
        ms,
        "int "
        "%S_parse_section(struct %S* s, struct c_ini_parser* p)\n"
        "{\n"
        "    enum token           tok;\n"
        "    struct c_ini_strspan key;\n"
        "\n"
        "    tok = scan_next(p);\n"
        "    while (1)\n"
        "    {\n"
        "        if (tok == TOK_ERROR) return TOK_ERROR;\n"
        "        if (tok == TOK_END) return TOK_END;\n"
        "        if (tok == TOK_KEY)\n"
        "        {\n"
        "            key = p->value.string;\n"
        "            if (0) {}\n",
        section->struct_name,
        section->struct_name);

    for (key = section->keys; key; key = key->next)
    {
        mstream_fmt(
            ms,
            "            else if (cstr_equal(\"%S\", key, p->source))\n",
            key->name);
        mstream_cstr(
            ms,
            "            {\n"
            "                if (scan_next(p) != '=')\n"
            "                    return parser_error(p, \"Expected \\\"=\\\" ");
        mstream_fmt(
            ms,
            "after key\\n\");\n"
            "                tok = parse_%S__%S(p, s);\n"
            "            }\n",
            section->struct_name,
            key->name);
    }

    mstream_fmt(
        ms,
        "            else\n"
        "            {\n"
        "                return parser_error(\n"
        "                    p, \"Unknown key \\\"%%.*s\\\" in section "
        "\\\"%S\\\"\\n\",\n"
        "                    key.len, p->source + key.off);\n"
        "            }\n"
        "            continue;\n"
        "        }\n"
        "\n"
        "        return tok;\n"
        "    }\n"
        "}\n\n",
        section->name);
}

static void gen_source_parse(struct mstream* ms, const struct section* section)
{
    mstream_fmt(
        ms,
        "static int %S_on_section(struct c_ini_parser* p, void* user_ptr)\n{\n",
        section->struct_name);
    mstream_fmt(
        ms,
        "    enum token tok = %S_parse_section(user_ptr, p);\n",
        section->struct_name);
    mstream_cstr(
        ms,
        "   if (tok != TOK_ERROR)\n"
        "        return TOK_END;\n"
        "    return tok;\n"
        "}\n\n");

    mstream_fmt(
        ms,
        "int %S_parse(\n"
        "    struct %S* s, const char* filename, const char* data, int "
        "len)\n{\n",
        section->struct_name,
        section->struct_name);
    mstream_fmt(
        ms,
        "    return %S_parse_all(filename, data, len, %S_on_section, s);\n",
        section->struct_name,
        section->struct_name);
    mstream_cstr(ms, "}\n\n");
}

static void
gen_source_parse_all(struct mstream* ms, const struct section* section)
{
    mstream_fmt(
        ms,
        "int %S_parse_all(\n"
        "    const char* filename,\n"
        "    const char* data,\n"
        "    int len,\n"
        "    int (*on_section)(struct c_ini_parser*, void*),\n"
        "    void* user_ptr)\n{\n",
        section->struct_name,
        section->struct_name);
    mstream_cstr(
        ms,
        "    struct c_ini_parser p;\n"
        "    parser_init(&p, filename, data, len);\n\n"
        "    while (1)\n"
        "    {\n"
        "        enum token tok = scan_next(&p);\n"
        "    reswitch_tok:\n"
        "        if (tok == TOK_ERROR) return -1;\n"
        "        if (tok == TOK_END) return 0;\n"
        "        if (tok == '[')\n"
        "        {\n");
    mstream_cstr(
        ms,
        "            if (scan_next(&p) != TOK_KEY)\n"
        "                return parser_error(\n"
        "                    &p,\n"
        "                    \"Expected a section name within the brackets. "
        "Example: \"\n"
        "                    \"[mysection]\\n\");\n");
    mstream_fmt(
        ms,
        "            if (!cstr_equal(\"%S\", p.value.string, data))\n"
        "                continue;\n",
        section->name);
    mstream_cstr(
        ms,
        "            if (scan_next(&p) != ']')\n"
        "                return parser_error(&p, \"Missing closing bracket "
        "\\\"]\\\"\\n\");\n");
    mstream_fmt(
        ms,
        "            tok = on_section(&p, user_ptr);\n"
        "            goto reswitch_tok;\n"
        "        }\n"
        "    }\n",
        section->struct_name);

    mstream_fmt(ms, "    return 0;\n", section->name);
    mstream_cstr(ms, "}\n\n");
}

static void
gen_source_for_each_value(struct mstream* ms, const struct section* section)
{
    const struct key* key;
    mstream_fmt(
        ms,
        "int %S_for_each_value(struct %S* s, int (*on_value)(void* value, "
        "int type, void*), void* user_ptr)\n{\n",
        section->struct_name,
        section->struct_name,
        section->struct_name);
    for (key = section->keys; key; key = key->next)
    {
        if (key->type & CDT_BITFIELD)
            continue;
        mstream_fmt(
            ms,
            "    if (on_value(&s->%S, %d, user_ptr) != 0)\n"
            "        return -1;\n",
            key->name,
            key->type);
    }
    mstream_cstr(ms, "    return 0;\n");
    mstream_cstr(ms, "}\n\n");
}

static void
gen_source(struct mstream* ms, const struct root* root, const struct cfg* cfg)
{
    const struct section* section;

    gen_source_includes(ms, cfg);
    gen_source_ini_parser(ms);
    gen_source_helpers(ms, root);

    for (section = root->sections; section; section = section->next)
    {
        gen_source_init(ms, section);
        gen_source_deinit(ms, section);
        gen_source_fwrite(ms, section);
        gen_source_parse_section(ms, section);
        gen_source_parse(ms, section);
        gen_source_parse_all(ms, section);
        gen_source_for_each_value(ms, section);
    }
}

static int write_if_different(const struct mstream* ms, const char* filename)
{
    struct mfile mf;

    /* Don't write resource if it is identical to the existing one -- causes
     * less rebuilds */
    if (mfile_map_read(&mf, filename, 1) == 0)
    {
        if (mf.size == ms->write_ptr &&
            memcmp(mf.address, ms->address, mf.size) == 0)
            return 0;
        mfile_unmap(&mf);
    }

    /* Write out file */
    if (mfile_map_write(&mf, filename, ms->write_ptr) != 0)
        return -1;
    memcpy(mf.address, ms->address, ms->write_ptr);
    mfile_unmap(&mf);

    return 0;
}

int main(int argc, char** argv)
{
    struct mfile   mf;
    struct mstream ms;
    struct parser  parser;
    struct cfg     cfg = {0};
    struct root    root = {0};

    if (!stream_is_terminal(stderr))
        disable_colors = 1;

    if (parse_cmdline(argc, argv, &cfg) != 0)
        return -1;

    if (cfg.input_fnames == NULL)
    {
        if (mfile_map_stdin(&mf) != 0)
            return -1;
        parser_init(&parser, &mf, "<stdin>");
        if (parse(&parser, &root, 0) != 0)
            return -1;
    }
    else
    {
        int i;
        for (i = 0; i != cfg.input_count; ++i)
        {
            char is_source_file = file_is_source_file(cfg.input_fnames[i]);
            if (mfile_map_read(&mf, cfg.input_fnames[i], 0) != 0)
                return -1;
            parser_init(&parser, &mf, cfg.input_fnames[i]);
            if (parse(&parser, &root, is_source_file) != 0)
                return -1;
        }
    }

    ms = mstream_init_writeable();
    gen_header(&ms, &root);
    if (cfg.output_header == NULL)
        fwrite(ms.address, ms.write_ptr, 1, stdout);
    else if (write_if_different(&ms, cfg.output_header) != 0)
        return -1;

    ms = mstream_init_writeable();
    gen_source(&ms, &root, &cfg);
    if (cfg.output_source == NULL)
        fwrite(ms.address, ms.write_ptr, 1, stdout);
    else if (write_if_different(&ms, cfg.output_source) != 0)
        return -1;

    return 0;
}
