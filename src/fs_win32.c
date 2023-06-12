#include "clither/fs.h"
#include "clither/utf8.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

/* ------------------------------------------------------------------------- */
void*
fs_map_file(const char* utf8_filename, int* length)
{
	HANDLE hFile;
    LARGE_INTEGER liFileSize;
    HANDLE mapping;
    void* address;
    wchar_t* utf16_filename;

    utf16_filename = utf8_to_utf16(utf8_filename, strlen(utf8_filename));
    if (utf16_filename == NULL)
        goto utf16_conv_failed;

    /* Try to open the file */
    hFile = CreateFileW(
        utf16_filename,         /* File name */
        GENERIC_READ,           /* Read only */
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,                   /* Default security */
        OPEN_EXISTING,          /* File must exist */
        FILE_ATTRIBUTE_NORMAL,  /* Default attributes */
        NULL);                  /* No attribute template */
    if (hFile == INVALID_HANDLE_VALUE)
        goto open_failed;

    /* Determine file size in bytes */
    if (!GetFileSizeEx(hFile, &liFileSize))
        goto get_file_size_failed;

    mapping = CreateFileMapping(
        hFile,                 /* File handle */
        NULL,                  /* Default security attributes */
        PAGE_READONLY,         /* Read only (or copy on write, but we don't write) */
        0, 0,                  /* High/Low size of mapping. Zero means entire file */
        NULL);                 /* Don't name the mapping */
    if (mapping == NULL)
        goto create_file_mapping_failed;

    address = MapViewOfFile(
        mapping,               /* File mapping handle */
        FILE_MAP_READ,         /* Read-only view of file */
        0, 0,                  /* High/Low offset of where the mapping should begin in the file */
        0);                    /* Length of mapping. Zero means entire file */
    if (address == NULL)
        goto map_view_failed;

    /* The file mapping isn't required anymore */
    CloseHandle(mapping);
    CloseHandle(hFile);
    utf16_free(utf16_filename);

    /* Success */
    *length = (int)liFileSize.QuadPart;
    return address;

map_view_failed:
    CloseHandle(mapping);
create_file_mapping_failed:
get_file_size_failed:
    CloseHandle(hFile);
open_failed:
    utf16_free(utf16_filename);
utf16_conv_failed:
    return NULL;
}

/* ------------------------------------------------------------------------- */
void
fs_unmap_file(void* address, int length)
{
    UnmapViewOfFile(address);
}
