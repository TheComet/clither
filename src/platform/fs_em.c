#include "clither/platform/fs.h"
#include "clither/util/log.h"
#include "clither/util/str.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */
int fs_get_path_to_self(struct str** path)
{
    return str_set_cstr(path, ".");
}

/* ------------------------------------------------------------------------- */
int fs_list(
    const char* path, int (*on_entry)(const char* name, void* user), void* user)
{
    DIR*           dp;
    struct dirent* ep;
    int            ret = 0;

    dp = opendir(path);
    if (!dp)
        goto first_file_failed;

    while ((ep = readdir(dp)) != NULL)
    {
        if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0)
            continue;
        ret = on_entry(ep->d_name, user);
        if (ret != 0)
            goto out;
    }

out:
    closedir(dp);
first_file_failed:
    return ret;
}

/* ------------------------------------------------------------------------- */
int fs_file_exists(const char* file_path)
{
    struct stat st;
    if (stat(file_path, &st))
        return 0;
    return S_ISREG(st.st_mode);
}

/* ------------------------------------------------------------------------- */
int fs_dir_exists(const char* file_path)
{
    struct stat st;
    if (stat(file_path, &st))
        return 0;
    return S_ISDIR(st.st_mode);
}

/* ------------------------------------------------------------------------- */
int fs_make_dir(const char* path)
{
    return (void)path, -1;
}

/* ------------------------------------------------------------------------- */
int fs_make_path(const char* path)
{
    return (void)path, -1;
}

/* ------------------------------------------------------------------------- */
int fs_appdata_dir(struct str** path)
{
    return str_set_cstr(path, ".");
}

/* ------------------------------------------------------------------------- */
struct fs_watch* fs_watch_init(void)
{
    return (struct fs_watch*)0x1;
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
