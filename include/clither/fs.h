#pragma once

void*
fs_map_file(const char* utf8_filename, int* length);

void
fs_unmap_file(void* address, int length);
