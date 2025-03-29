#include "clither/platform/fs.h"
#include "clither/util/log.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <sys/inotify.h>
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

int fs_file_exists(const char* file_path)
{
    struct stat st;
    if (stat(file_path, &st))
        return 0;
    return S_ISREG(st.st_mode);
}

int fs_dir_exists(const char* file_path)
{
    struct stat st;
    if (stat(file_path, &st))
        return 0;
    return S_ISDIR(st.st_mode);
}

struct fs_watch* fs_watch_init(void)
{
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0)
    {
        log_err("inotify_init() failed: %s\n", strerror(errno));
        return NULL;
    }
    return (struct fs_watch*)(intptr_t)fd;
}

void fs_watch_deinit(struct fs_watch* w)
{
    int fd = (int)(intptr_t)w;
    close(fd);
}

int fs_watch_file(struct fs_watch* w, const char* path)
{
    int fd = (int)(intptr_t)w;

    if (inotify_add_watch(fd, path, IN_ALL_EVENTS) < 0)
        return log_err("inotify_add_watch() failed: %s\n", strerror(errno));

    return 0;
}

int fs_watch_check(struct fs_watch* w)
{
    char* ptr;
    char  buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    struct inotify_event* event;
    ssize_t               size;
    int                   fd = (int)(intptr_t)w;
    int                   modified_flag = 0;

    while (1)
    {
        size = read(fd, buf, sizeof(buf));
        if (size < 0 && errno != EAGAIN)
        {
            log_err("read() failed: %s\n", strerror(errno));
            return -1;
        }
        if (size <= 0)
            break;

        for (ptr = buf; ptr < buf + size;)
        {
            event = (struct inotify_event*)ptr;
            if (event->mask &
                (IN_MODIFY | IN_CLOSE_WRITE | IN_MOVE_SELF | IN_MOVE))
            {
                modified_flag = 1;
            }
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }

    return modified_flag;
}
