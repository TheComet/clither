#pragma once

#include <string.h>

struct strview
{
    const char* data;
    int         off, len;
};

static struct strview cstr_view(const char* cstr)
{
    struct strview view;
    view.data = cstr;
    view.off = 0;
    view.len = (int)strlen(cstr);
    return view;
}

static struct strview strview(const char* data, int off, int len)
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
int   strview_eq(struct strview s1, struct strview s2);

static const char* strview_cstr(struct strview str)
{
    return str.data + str.off;
}
static const char* strview_data(struct strview str)
{
    return str.data + str.off;
}
static int strview_len(struct strview str)
{
    return str.len;
}
