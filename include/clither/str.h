#pragma once

#include "clither/strview.h"
#include "clither/vec.h"

struct str;
VEC_DECLARE(str_impl, char, 16)

void str_init(struct str** str);
void str_deinit(struct str* str);
int  str_set_cstr(struct str** str, const char* cstr);
int  str_join_path(struct str** str, struct strview path);
int  str_join_path_cstr(struct str** str, const char* path);

static const char* str_cstr(const struct str* str)
{
    return str ? ((const struct str_impl*)str)->data : "";
}

static int str_len(const struct str* str)
{
    return str ? ((const struct str_impl*)str)->count - 1 : 0;
}

int cstr_ends_with(const char* cstr, const char* suffix);
