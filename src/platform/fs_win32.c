#include "clither/platform/fs.h"
#include "clither/util/str.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <KnownFolders.h>
#include <ShlObj.h>

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

int
fs_file_exists(const char* path)
{
    DWORD attr = GetFileAttributes(path);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return 0;
    return !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

int
fs_dir_exists(const char* path)
{
    DWORD attr = GetFileAttributes(path);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return 0;
    return !!(attr & FILE_ATTRIBUTE_DIRECTORY);
}

struct fs_watch* fs_watch_init(void)
{
    return (void*)1;
}

void fs_watch_deinit(struct fs_watch* w)
{
}

int fs_watch_file(struct fs_watch* w, const char* path)
{
    return 1;
}

int fs_watch_check(struct fs_watch* w)
{
    return 0;
}
