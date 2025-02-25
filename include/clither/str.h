#pragma once

struct str;

struct strspan
{
    int off, len;
};

static inline struct strspan strspan(int off, int len)
{
    struct strspan span;
    span.off = off;
    span.len = len;
    return span;
}

struct strview
{
    const char* data;
    int         off, len;
};

static inline struct strview strview(const char* data, int off, int len)
{
    struct strview view;
    view.data = data;
    view.off = off;
    view.len = len;
    return view;
}
float strview_to_float(struct strview str);
int   strview_to_integer(struct strview str);
int   strview_eq_cstr(struct strview str, const char* cstr);

void        str_init(struct str** str);
void        str_deinit(struct str* str);
int         str_len(const struct str* str);
int         str_set_cstr(struct str** str, const char* cstr);
int         str_join_path_cstr(struct str** str, const char* path);
const char* str_cstr(const struct str* str);
