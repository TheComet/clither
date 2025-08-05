#include "clither/platform/fs.h"
#include "clither/util/log.h"
#include "clither/util/str.h"
#include "clither/util/tracker.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <pwd.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */
int fs_get_path_to_self(struct str** path)
{
    if (str_ensure_capacity(path, 256) != 0)
        return -1;

    while (1)
    {
        /* NOTE: readlink() does NOT null-terminate the resulting string, which
         * is fine for struct utf8 since it doesn't expect it to be, but we
         * still have to make sure to reserve the space for a NULL terminator at
         * the end of the buffer */
        int len =
            readlink("/proc/self/exe", str_data(*path), str_capacity(*path));
        if (len < 0)
        {
            return log_err(
                "readlink() failed in fs_get_path_to_self(): %s",
                strerror(errno));
        }

        if (len < str_capacity(*path))
            break;

        if (str_ensure_capacity(path, str_capacity(*path) * 2) != 0)
            return -1;
    }

    return 0;
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
    if (mkdir(path, 0755) == 0)
        return 0;

    if (errno == EEXIST)
        return 1;

    log_err(
        "Failed to create directory {quote:%s}: %s\n", path, strerror(errno));
    return -1;
}

/* ------------------------------------------------------------------------- */
static int make_path_impl(struct str* intermediary)
{
    while (1)
    {
        if (mkdir(str_cstr(intermediary), 0755) == 0)
            return 0;

        if (errno == EEXIST)
            return 0;

        if (errno == ENOENT)
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
        log_err("Failed to create path %s: %s\n", path, strerror(errno));
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
    struct passwd* pw = getpwuid(getuid());
    if (pw == NULL)
        return log_err("Failed to get user info: %s\n", strerror(errno));

    if (str_set_path_cstr(path, pw->pw_dir) != 0)
        return -1;
    if (str_join_path_cstr(path, ".local/share") != 0)
        return -1;

    return 0;
}

/* ------------------------------------------------------------------------- */
struct fs_watch* fs_watch_init(void)
{
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0)
    {
        log_err("inotify_init() failed: %s\n", strerror(errno));
        return NULL;
    }
    track_fd(fd);
    return (struct fs_watch*)(intptr_t)fd;
}

/* ------------------------------------------------------------------------- */
void fs_watch_deinit(struct fs_watch* w)
{
    int fd = (int)(intptr_t)w;
    untrack_fd(fd);
    close(fd);
}

/* ------------------------------------------------------------------------- */
int fs_watch_file(struct fs_watch* w, const char* path)
{
    int fd = (int)(intptr_t)w;

    if (inotify_add_watch(fd, path, IN_ALL_EVENTS) < 0)
        return log_err("inotify_add_watch() failed: %s\n", strerror(errno));

    return 0;
}

/* ------------------------------------------------------------------------- */
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
