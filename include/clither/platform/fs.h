#pragma once

#include "clither/config.h"

struct fs_watch;

int fs_list(
    const char* path,
    int (*on_entry)(const char* name, void* user),
    void* user);
int fs_file_exists(const char* path);
int fs_dir_exists(const char* path);

struct fs_watch* fs_watch_init(void);
void             fs_watch_deinit(struct fs_watch* w);
int              fs_watch_file(struct fs_watch* w, const char* path);
int              fs_watch_check(struct fs_watch* w);
