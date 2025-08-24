#pragma once

#include "clither/util/strview.h"
#include <stdint.h>

struct str;

int         str_init(struct str** str);
void        str_deinit(struct str* str);
int         str_ensure_capacity(struct str** str, int capacity);
int         str_capacity(const struct str* str);
int         str_len(const struct str* str);
char*       str_data(struct str* str);
int         str_append_char(struct str** str, char c);
int         str_append_cstr(struct str** str, const char* cstr);
int         str_append_int(struct str** str, int value);
void        str_pop_char(struct str* str);
void        str_set_char(struct str* str, int index, char c);
void        str_clear(struct str* str);
void        str_set_len(struct str* str, int new_len);
int         str_set(struct str** str, const char* data, int len);
int         str_set_str(struct str** str, const struct str* other);
int         str_set_view(struct str** str, struct strview view);
int         str_set_utf32(struct str** str, const uint32_t* utf32, int len);
int         str_set_cstr(struct str** str, const char* cstr);
const char* str_cstr(const struct str* str);
int         str_eq_cstr(const struct str* str, const char* cstr);

int         str_set_path_cstr(struct str** str, const char* path);
int         str_join_path(struct str** str, struct strview path);
int         str_join_path_cstr(struct str** str, const char* path);
int         str_join_path_prepend_cstr(struct str** str, const char* path);
void        str_dirname(struct str* str);
const char* cstr_ext(const char* str);

int cstr_ends_with(const char* cstr, const char* suffix);

static struct strview str_view(const struct str* str)
{
    return strview(str_cstr(str), 0, str_len(str));
}
