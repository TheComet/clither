#include "clither/platform/fs.h"
#include "clither/platform/utf8.h"
#include "clither/util/log.h"
#include "clither/util/str.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* must be included after windows.h */
#include <knownfolders.h>
#include <shlobj.h>

/* ------------------------------------------------------------------------- */
int fs_list(
    const char* path, int (*on_entry)(const char* name, void* user), void* user)
{
    DWORD           dwError;
    WIN32_FIND_DATA ffd;
    int             ret = 0;
    HANDLE          hFind = INVALID_HANDLE_VALUE;
    struct str*     correct_path;

    str_init(&correct_path);
    if (str_set_path_cstr(&correct_path, path) != 0)
        goto str_set_failed;
    if (str_join_path_cstr(&correct_path, "*") != 0)
        goto first_file_failed;

    hFind = FindFirstFileA(str_cstr(correct_path), &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
        goto first_file_failed;

    do
    {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
            continue;
        ret = on_entry(ffd.cFileName, user);
        if (ret != 0)
            goto out;
    } while (FindNextFile(hFind, &ffd) != 0);

    dwError = GetLastError();
    if (dwError != ERROR_NO_MORE_FILES)
        ret = -1;

out:
    FindClose(hFind);
first_file_failed:
    str_deinit(correct_path);
str_set_failed:
    return ret;
}

/* ------------------------------------------------------------------------- */
int fs_file_exists(const char* path)
{
    DWORD attr = GetFileAttributes(path);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return 0;
    return !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

/* ------------------------------------------------------------------------- */
int fs_dir_exists(const char* path)
{
    DWORD attr = GetFileAttributes(path);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return 0;
    return !!(attr & FILE_ATTRIBUTE_DIRECTORY);
}

/* ------------------------------------------------------------------------- */
int fs_make_dir(const char* path)
{
    if (CreateDirectory(path, NULL))
        return 0;
    return -1;
}

/* ------------------------------------------------------------------------- */
static int make_path_impl(struct str* intermediary)
{
    while (1)
    {
        if (CreateDirectory(str_cstr(intermediary), NULL))
            return 0;

        if (GetLastError() == ERROR_ALREADY_EXISTS)
            return 0;

        if (GetLastError() == ERROR_PATH_NOT_FOUND)
        {
            int result;
            int len_store = str_len(intermediary);

            str_dirname(intermediary);
            result = make_path_impl(intermediary);

            str_set_char(intermediary, str_len(intermediary), '/');
            str_set_len(intermediary, len_store);
            if (result == 0)
                continue;
        }

        return -1;
    }
}
int fs_make_path(const char* path)
{
    struct str* intermediary;
    str_init(&intermediary);
    if (str_set_cstr(&intermediary, path) != 0)
        goto failed;

    if (make_path_impl(intermediary) != 0)
    {
        log_err_win32("Failed to create path %s\n", path);
        goto failed;
    }

    str_deinit(intermediary);
    return 0;

failed:
    str_deinit(intermediary);
    return -1;
}

/* ------------------------------------------------------------------------- */
int fs_appdata_dir(struct str** path)
{
    PWSTR   utf16_path = NULL;
    HRESULT hr =
        SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &utf16_path);
    if (FAILED(hr))
        goto get_folder_failed;

    if (utf16_to_utf8(path, utf16_path, (int)wcslen(utf16_path)) != 0)
        goto utf_conversion_failed;

    CoTaskMemFree(utf16_path);
    return 0;

utf_conversion_failed:
    CoTaskMemFree(utf16_path);
get_folder_failed:
    return -1;
}

/* ------------------------------------------------------------------------- */
struct fs_watch* fs_watch_init(void)
{
    return (void*)1;
}

/* ------------------------------------------------------------------------- */
void fs_watch_deinit(struct fs_watch* w)
{
    (void)w;
}

/* ------------------------------------------------------------------------- */
int fs_watch_file(struct fs_watch* w, const char* path)
{
    (void)w, (void)path;
    return 0;
}

/* ------------------------------------------------------------------------- */
int fs_watch_check(struct fs_watch* w)
{
    (void)w;
    return 0;
}
