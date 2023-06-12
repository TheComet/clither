#include "clither/utf8.h"
#include "cstructures/memory.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

/* ------------------------------------------------------------------------- */
wchar_t* utf8_to_utf16(const char* utf8, int utf8_bytes)
{
    wchar_t* utf16;
    int utf16_bytes = MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_bytes, NULL, 0);
    if (utf16_bytes == 0)
        return NULL;

    utf16 = (wchar_t*)MALLOC((sizeof(wchar_t) + 1) * utf16_bytes);
    if (utf16 == NULL)
        return NULL;

    if (MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_bytes, utf16, utf16_bytes) == 0)
    {
        FREE(utf16);
        return NULL;
    }

    utf16[utf16_bytes] = 0;

    return utf16;
}

/* ------------------------------------------------------------------------- */
void utf16_free(wchar_t* utf16)
{
    FREE(utf16);
}

/* ------------------------------------------------------------------------- */
FILE* utf8_fopen_wb(const char* utf8_filename, int utf8_filename_bytes)
{
    FILE* fp;
    wchar_t* utf16_filename = utf8_to_utf16(utf8_filename, utf8_filename_bytes);
    if (utf16_filename == NULL)
        return NULL;

    fp = _wfopen(utf16_filename, L"wb");
    FREE(utf16_filename);

    return fp;
}

/* ------------------------------------------------------------------------- */
FILE* utf8_fopen_rb(const char* utf8_filename, int utf8_filename_bytes)
{
    FILE* fp;
    wchar_t* utf16_filename = utf8_to_utf16(utf8_filename, utf8_filename_bytes);
    if (utf16_filename == NULL)
        return NULL;

    fp = _wfopen(utf16_filename, L"rb");
    FREE(utf16_filename);

    return fp;
}

/* ------------------------------------------------------------------------- */
int utf8_remove(const char* utf8_filename, int utf8_filename_bytes)
{
    int result;
    wchar_t* utf16_filename = utf8_to_utf16(utf8_filename, utf8_filename_bytes);
    if (utf16_filename == NULL)
        return -1;

    result = _wremove(utf16_filename);
    FREE(utf16_filename);
    return result;
}
