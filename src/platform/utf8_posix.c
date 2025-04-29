#include "clither/platform/utf8.h"
#include "clither/util/mem.h"
#include <assert.h>
#include <stdio.h>

/* ------------------------------------------------------------------------- */
wchar_t* utf8_to_utf16(const char* utf8, int utf8_bytes)
{
    (void)utf8;
    (void)utf8_bytes;

    return NULL;
}

/* ------------------------------------------------------------------------- */
void utf16_free(wchar_t* utf16)
{
    (void)utf16;
    assert(0);
}

/* ------------------------------------------------------------------------- */
FILE* utf8_fopen_wb(const char* utf8_filename, int utf8_filename_bytes)
{
    FILE* fp;
    (void)utf8_filename_bytes;
    fp = fopen(utf8_filename, "wb");
    mem_track_allocation(fp, 0);
    return fp;
}

/* ------------------------------------------------------------------------- */
FILE* utf8_fopen_rb(const char* utf8_filename, int utf8_filename_bytes)
{
    FILE* fp;
    (void)utf8_filename_bytes;
    fp = fopen(utf8_filename, "rb");
    mem_track_allocation(fp, 0);
    return fp;
}

/* ------------------------------------------------------------------------- */
void utf8_fclose(FILE* fp)
{
    mem_track_deallocation(fp);
    fclose(fp);
}

/* ------------------------------------------------------------------------- */
int utf8_remove(const char* utf8_filename, int utf8_filename_bytes)
{
    (void)utf8_filename_bytes;
    return remove(utf8_filename);
}
