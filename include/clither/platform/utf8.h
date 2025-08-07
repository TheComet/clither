#pragma once

#include <stddef.h>
#include <stdio.h>

struct str;

wchar_t* utf8_to_utf16(const char* utf8, int utf8_bytes);
int      utf16_to_utf8(struct str** utf8, const wchar_t* utf16, int utf16_len);
void     utf16_free(wchar_t* utf16);

FILE* utf8_fopen_wb(const char* utf8_filename, int utf8_filename_bytes);
FILE* utf8_fopen_rb(const char* utf8_filename, int utf8_filename_bytes);
void  utf8_fclose(FILE* fp);
