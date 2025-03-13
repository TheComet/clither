#pragma once

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
