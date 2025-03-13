#include "clither/fs.h"
#include <assert.h>
#include <dirent.h>
#include <pwd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

int fs_file_exists(const char* path)
{
    struct stat st;
    if (stat(path, &st))
        return 0;
    return S_ISREG(st.st_mode);
}

int fs_dir_exists(const char* path)
{
    struct stat st;
    if (stat(path, &st))
        return 0;
    return S_ISDIR(st.st_mode);
}
