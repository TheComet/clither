#include "clither/util/str.h"

#if defined(_WIN32)
#    define SEP     '\\'
#    define BAD_SEP '/'
#else
#    define SEP     '/'
#    define BAD_SEP '\\'
#endif

VEC_DEFINE(str_impl, char, 16)

void str_init(struct str** str)
{
    str_impl_init((struct str_impl**)str);
}

void str_deinit(struct str* str)
{
    str_impl_deinit((struct str_impl*)str);
}

int str_set_cstr(struct str** str, const char* cstr)
{
    struct str_impl* impl;
    int              len = (int)strlen(cstr);

    if (vec_capacity((struct str_impl*)*str) < len + 1)
        if (str_impl_realloc((struct str_impl**)str, len + 1) != 0)
            return -1;

    impl = (struct str_impl*)*str;
    memcpy(impl->data, cstr, len);
    impl->data[len] = '\0';
    impl->count = len + 1;

    return 0;
}

int str_set_path_cstr(struct str** str, const char* path)
{
    int              i;
    struct str_impl* impl;
    int              len = (int)strlen(path);

    if (vec_capacity((struct str_impl*)*str) < len + 1)
        if (str_impl_realloc((struct str_impl**)str, len + 1) != 0)
            return -1;

    impl = (struct str_impl*)*str;
    memcpy(impl->data, path, len);
    impl->data[len] = '\0';
    impl->count = len + 1;

    for (i = 0; i != len; ++i)
        if (impl->data[i] == BAD_SEP)
            impl->data[i] = SEP;

    return 0;
}

int str_join_path(struct str** str, struct strview path)
{
    struct str_impl* impl;
    int              len = str_len(*str);

    if (vec_capacity((struct str_impl*)*str) < len + 1 + path.len + 1)
        if (str_impl_realloc((struct str_impl**)str, len + 1 + path.len + 1) !=
            0)
        {
            return -1;
        }
    impl = (struct str_impl*)*str;

    if (impl->data[len] != SEP)
    {
        impl->data[len] = SEP;
        len++;
    }

    memcpy(impl->data + len, path.data + path.off, path.len);
    len += path.len;

    impl->data[len] = '\0';
    impl->count = len + 1;

    return 0;
}

int str_join_path_cstr(struct str** str, const char* path)
{
    return str_join_path(str, strview(path, 0, (int)strlen(path)));
}

int cstr_ends_with(const char* cstr, const char* suffix)
{
    int cstr_len = (int)strlen(cstr);
    int suffix_len = (int)strlen(suffix);

    if (cstr_len < suffix_len)
        return 0;

    return strcmp(cstr + cstr_len - suffix_len, suffix) == 0;
}
