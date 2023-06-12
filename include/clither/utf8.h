#pragma once

#include <stdio.h>

wchar_t*
utf8_to_utf16(const char* utf8, int utf8_bytes);
void
utf16_free(wchar_t* utf16);

FILE*
utf8_fopen_wb(const char* utf8_filename, int utf8_filename_bytes);
FILE*
utf8_fopen_rb(const char* utf8_filename, int utf8_filename_bytes);
int
utf8_remove(const char* utf8_filename, int utf8_filename_bytes);
