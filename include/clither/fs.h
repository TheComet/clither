#pragma once

/*!
 * @brief Returns the absolute path to the executable file calling this
 * function, independent of the current working directory.
 * @note The returned path needs to be freed using @see ospath_deinit().
 * @param[out] path Structure receiving the resulting path. Must be initialized.
 * @return Returns 0 on success, negative on error;
 */
int
fs_get_path_to_self(const char* path);

int
fs_list(
    const char* path,
    int (*on_entry)(const char* name, void* user),
    void* user);

int
fs_file_exists(const char* path);

int
fs_dir_exists(const char* path);

