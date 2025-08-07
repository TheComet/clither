#include "clither/platform/utf8.h"
#include "clither/util/mem.h"
#include "clither/util/str.h"
#include "clither/util/tracker.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ------------------------------------------------------------------------- */
wchar_t* utf8_to_utf16(const char* utf8, int utf8_bytes)
{
    wchar_t* utf16;
    int      utf16_bytes =
        MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_bytes, NULL, 0);
    if (utf16_bytes == 0)
        return NULL;

    utf16 = (wchar_t*)mem_alloc((sizeof(wchar_t) + 1) * utf16_bytes);
    if (utf16 == NULL)
        return NULL;

    if (MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_bytes, utf16, utf16_bytes) ==
        0)
    {
        mem_free(utf16);
        return NULL;
    }

    utf16[utf16_bytes] = 0;

    return utf16;
}

/* ------------------------------------------------------------------------- */
int utf16_to_utf8(struct str** utf8, const wchar_t* utf16, int utf16_len)
{
    int utf8_bytes =
        WideCharToMultiByte(CP_UTF8, 0, utf16, utf16_len, NULL, 0, NULL, NULL);
    if (utf8_bytes == 0)
        return -1;

    if (str_ensure_capacity(utf8, utf8_bytes + 1) != 0)
        return -1;

    if (WideCharToMultiByte(
            CP_UTF8,
            0,
            utf16,
            utf16_len,
            str_data(*utf8),
            utf8_bytes,
            NULL,
            NULL) == 0)
    {
        return -1;
    }

    str_set_len(*utf8, utf8_bytes);
    return 0;
}

/* ------------------------------------------------------------------------- */
void utf16_free(wchar_t* utf16)
{
    mem_free(utf16);
}

/* ------------------------------------------------------------------------- */
FILE* utf8_fopen_wb(const char* utf8_filename, int utf8_filename_bytes)
{
    FILE*    fp;
    wchar_t* utf16_filename = utf8_to_utf16(utf8_filename, utf8_filename_bytes);
    if (utf16_filename == NULL)
        return NULL;

    fp = _wfopen(utf16_filename, L"wb");
    mem_free(utf16_filename);
    track_mem(fp, 0, utf8_filename);

    return fp;
}

/* ------------------------------------------------------------------------- */
FILE* utf8_fopen_rb(const char* utf8_filename, int utf8_filename_bytes)
{
    FILE*    fp;
    wchar_t* utf16_filename = utf8_to_utf16(utf8_filename, utf8_filename_bytes);
    if (utf16_filename == NULL)
        return NULL;

    fp = _wfopen(utf16_filename, L"rb");
    mem_free(utf16_filename);
    track_mem(fp, 0, utf8_filename);

    return fp;
}

/* ------------------------------------------------------------------------- */
void utf8_fclose(FILE* fp)
{
    untrack_mem(fp);
    fclose(fp);
}
