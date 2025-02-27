#include "clither/config.h"
#include "clither/log.h"
#include "clither/mem.h"
#include "clither/str.h"

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
    int              len = strlen(cstr);

    if (vec_capacity((struct str_impl*)*str) < len + 1)
        if (str_impl_realloc((struct str_impl**)str, len + 1) != 0)
            return -1;

    impl = (struct str_impl*)*str;
    memcpy(impl->data, cstr, len);
    impl->data[len] = '\0';
    impl->count = len + 1;

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
    if (impl->data[len] != '/')
    {
        impl->data[len] = '/';
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
    return str_join_path(str, strview(path, 0, strlen(path)));
}

int cstr_ends_with(const char* cstr, const char* suffix)
{
    int cstr_len = strlen(cstr);
    int suffix_len = strlen(suffix);

    if (cstr_len < suffix_len)
        return 0;

    return strcmp(cstr + cstr_len - suffix_len, suffix) == 0;
}
