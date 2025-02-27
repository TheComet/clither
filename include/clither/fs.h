#pragma once

int fs_list(
    const char* path,
    int (*on_entry)(const char* name, void* user),
    void* user);

int fs_file_exists(const char* path);

int fs_dir_exists(const char* path);
