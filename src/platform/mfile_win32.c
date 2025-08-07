#include "clither/platform/mfile.h"
#include "clither/platform/utf8.h"
#include "clither/util/log.h"
#include "clither/util/tracker.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ------------------------------------------------------------------------- */
int mfile_map_read(struct mfile* mf, const char* filepath, int log_error)
{
    HANDLE        hFile;
    LARGE_INTEGER liFileSize;
    HANDLE        hMapping;
    wchar_t*      utf16_filename;

    utf16_filename = utf8_to_utf16(filepath, (int)strlen(filepath));
    if (utf16_filename == NULL)
        goto utf16_conv_failed;

    /* Try to open the file */
    hFile = CreateFileW(
        utf16_filename, /* File name */
        GENERIC_READ,   /* Read only */
        FILE_SHARE_READ,
        NULL,                  /* Default security */
        OPEN_EXISTING,         /* File must exist */
        FILE_ATTRIBUTE_NORMAL, /* Default attributes */
        NULL);                 /* No attribute template */
    if (hFile == INVALID_HANDLE_VALUE)
    {
        if (log_error)
            log_err_win32("Failed to open file \"%s\"\n", filepath);
        goto open_failed;
    }

    /* Determine file size in bytes */
    if (!GetFileSizeEx(hFile, &liFileSize))
        goto get_file_size_failed;
    if (liFileSize.QuadPart >
        (LONGLONG)((1U << 31) - 1)) /* mf->size is an int */
    {
        log_err(
            "Failed to map file \"%s\": Mapping files >4GiB is not "
            "implemented\n",
            filepath);
        goto get_file_size_failed;
    }
    mf->size = (int)liFileSize.LowPart;

    hMapping = CreateFileMappingW(
        hFile,         /* File handle */
        NULL,          /* Default security attributes */
        PAGE_READONLY, /* Read-only */
        0,
        mf->size, /* High/Low size of mapping. Zero means entire file */
        NULL);    /* Don't name the mapping */
    if (hMapping == NULL)
    {
        log_err_win32(
            "Failed to create file mapping for file \"%s\"\n", filepath);
        goto create_file_mapping_failed;
    }

    mf->address = MapViewOfFile(
        hMapping,      /* File mapping handle */
        FILE_MAP_READ, /* Read-Only */
        0,
        0,  /* High/Low offset of where the mapping should begin in the file */
        0); /* Length of mapping. Zero means entire file */
    if (mf->address == NULL)
    {
        log_err_win32("Failed to map view of file \"%s\"\n", filepath);
        goto map_view_failed;
    }

    track_mem(mf->address, mf->size, filepath);

    /* Don't need these anymore */
    CloseHandle(hMapping);
    CloseHandle(hFile);
    utf16_free(utf16_filename);

    return 0;

map_view_failed:
create_file_mapping_failed:
    CloseHandle(hMapping);
get_file_size_failed:
    CloseHandle(hFile);
open_failed:
    utf16_free(utf16_filename);
utf16_conv_failed:
    return -1;
}

/* ------------------------------------------------------------------------- */
int mfile_map_overwrite(struct mfile* mf, int size, const char* filepath)
{
    HANDLE   hFile;
    HANDLE   hMapping;
    wchar_t* utf16_filename;

    utf16_filename = utf8_to_utf16(filepath, (int)strlen(filepath));
    if (utf16_filename == NULL)
        goto utf16_conv_failed;

    /* Try to open the file */
    hFile = CreateFileW(
        utf16_filename,               /* File name */
        GENERIC_READ | GENERIC_WRITE, /* Read/write */
        0,
        NULL,                  /* Default security */
        CREATE_ALWAYS,         /* Overwrite any existing, otherwise create */
        FILE_ATTRIBUTE_NORMAL, /* Default attributes */
        NULL);                 /* No attribute template */
    if (hFile == INVALID_HANDLE_VALUE)
    {
        log_err_win32("Failed to open file \"%s\"\n", filepath);
        goto open_failed;
    }

    hMapping = CreateFileMappingW(
        hFile,          /* File handle */
        NULL,           /* Default security attributes */
        PAGE_READWRITE, /* Read + Write */
        0,
        size,  /* High/Low size of mapping */
        NULL); /* Don't name the mapping */
    if (hMapping == NULL)
    {
        log_err_win32(
            "Failed to create file mapping for file \"%s\"\n", filepath);
        goto create_file_mapping_failed;
    }

    mf->address = MapViewOfFile(
        hMapping,                       /* File mapping handle */
        FILE_MAP_READ | FILE_MAP_WRITE, /* Read + Write */
        0,
        0,  /* High/Low offset of where the mapping should begin in the file */
        0); /* Length of mapping. Zero means entire file */
    if (mf->address == NULL)
    {
        log_err_win32("Failed to map view of file \"%s\"\n", filepath);
        goto map_view_failed;
    }

    track_mem(mf->address, mf->size, filepath);

    /* Don't need these anymore */
    CloseHandle(hMapping);
    CloseHandle(hFile);
    utf16_free(utf16_filename);

    return 0;

map_view_failed:
    CloseHandle(hMapping);
create_file_mapping_failed:
    CloseHandle(hFile);
open_failed:
    utf16_free(utf16_filename);
utf16_conv_failed:
    return -1;
}

/* ------------------------------------------------------------------------- */
int mfile_map_mem(struct mfile* mf, int size)
{
    HANDLE mapping = CreateFileMapping(
        INVALID_HANDLE_VALUE, /* File handle */
        NULL,                 /* Default security attributes */
        PAGE_READWRITE,       /* Read + Write access */
        0,
        size,  /* High/Low size of mapping. Zero means entire file */
        NULL); /* Don't name the mapping */
    if (mapping == NULL)
    {
        log_err_win32("Failed to create file mapping of size %d\n", size);
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
        log_err_win32("Failed to map memory of size %d\n", size);
        goto map_view_failed;
    }

    CloseHandle(mapping);

    mf->size = size;
    track_mem(mf->address, mf->size, "mfile_map_mem");

    return 0;

map_view_failed:
    CloseHandle(mapping);
create_file_mapping_failed:
    return -1;
}

/* ------------------------------------------------------------------------- */
void mfile_unmap(struct mfile* mf)
{
    untrack_mem(mf->address);
    UnmapViewOfFile(mf->address);
}
