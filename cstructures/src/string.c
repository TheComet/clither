#include "cstructures/string.h"
#include "cstructures/vector.h"
#include "cstructures/memory.h"
#include <assert.h>
#include <string.h>
#include <stdarg.h>

/* ------------------------------------------------------------------------- */
struct cs_string*
string_create(void)
{
    struct cs_string* str = MALLOC(sizeof *str);
    if (str == NULL)
        return NULL;

    string_init(str);
    return 0;
}

/* ------------------------------------------------------------------------- */
void
string_init(struct cs_string* str)
{
    vector_init(&str->buf, sizeof(char));
}

/* ------------------------------------------------------------------------- */
void
string_deinit(struct cs_string* str)
{
    vector_deinit(&str->buf);
}

/* ------------------------------------------------------------------------- */
void
string_destroy(struct cs_string* str)
{
    string_deinit(str);
    FREE(str);
}

/* ------------------------------------------------------------------------- */
void
string_set(struct cs_string* str, const char* c)
{
    int len = (int)strlen(c);
    if ((int)vector_count(&str->buf) < len + 1)
        vector_resize(&str->buf, len + 1);
    strcpy((char*)vector_data(&str->buf), c);
}

/* ------------------------------------------------------------------------- */
struct cs_string*
string_cat(struct cs_string* str, const char* c_str)
{
    int c_len = (int)strlen(c_str);
    int len = string_length(str);
    vector_resize(&str->buf, len + c_len + 1);
    strcpy((char*)vector_data(&str->buf) + len, c_str);
    return str;
}

/* ------------------------------------------------------------------------- */
struct cs_string*
string_cat2(struct cs_string* str, const char* s1, const char* s2)
{
    int len1 = (int)strlen(s1);
    int len2 = (int)strlen(s2);
    vector_resize(&str->buf, len1 + len2 + 1);
    strcpy((char*)vector_data(&str->buf), s1);
    strcpy((char*)vector_data(&str->buf) + len1, s2);
    return str;
}

/* ------------------------------------------------------------------------- */
struct cs_string*
string_join(struct cs_string* str, int n, ...)
{
    va_list va;
    va_start(va, n);
    while (n--)
        string_cat(str, va_arg(va, const char*));
    va_end(va);
    return str;
}

/* ------------------------------------------------------------------------- */
char*
string_take(struct cs_string* str)
{
    char* ret = (char*)vector_data(&str->buf);
    str->buf.data = NULL;
    vector_compact(&str->buf);
    return ret;
}

/* ------------------------------------------------------------------------- */
int
string_getline(struct cs_string* str, FILE* fp)
{
    int ret;
    assert(fp);

    vector_clear(&str->buf);
    while ((ret = fgetc(fp)) != EOF)
    {
        char c = (char)ret;

        /* Ignore carriage */
        if (c == '\r')
            continue;

        /* If we encounter a newline, complete string and return */
        if (c == '\n')
        {
            /* empty line? */
            if (vector_count(&str->buf) == 0)
                continue;

            char nullterm = '\0';
            if (vector_push(&str->buf, &nullterm) != 0)
                return -1;
            return 1;
        }

        if (vector_push(&str->buf, &c) != 0)
            return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
char*
string_tok(struct cs_string* str, char delimiter, char** saveptr)
{
    char* begin_ptr;

    if (str)
        *saveptr = (char*)vector_data(&str->buf) - 1;
    else if (!*saveptr)
        return NULL;  /* no more tokens */

    /* get first occurrence of token in string */
    begin_ptr = *saveptr + 1;
    *saveptr = (char*)strchr(begin_ptr, delimiter);
    if(*saveptr)
        **saveptr = '\0';

    /* empty tokens */
    if(*begin_ptr == '\0')
        return NULL;
    return begin_ptr;
}

/* ------------------------------------------------------------------------- */
char*
string_tok_strip(struct cs_string* str, char delimiter, char strip, char** saveptr)
{
    return string_tok_strip_c(string_cstr(str), delimiter, strip, saveptr);
}

/* ------------------------------------------------------------------------- */
char*
string_tok_strip_c(char* str, char delimiter, char strip, char** saveptr)
{
    char* begin_ptr;
    char* end_ptr;

    if (str)
        *saveptr = str - 1;
    else if (!*saveptr)
        return NULL;  /* no more tokens */

    /* get first occurrence of token in string */
    begin_ptr = *saveptr + 1;
    *saveptr = (char*)strchr(begin_ptr, delimiter);
    if (*saveptr)
        **saveptr = '\0';

    /* empty tokens */
    if (*begin_ptr == '\0')
        return NULL;

    while (*begin_ptr && *begin_ptr == strip)
        *begin_ptr++ = '\0';
    end_ptr = *saveptr ? *saveptr - 1 : begin_ptr + strlen(begin_ptr) - 1;
    while (*end_ptr && *end_ptr == strip)
        *end_ptr-- = '\0';
    return begin_ptr;
}

/* ------------------------------------------------------------------------- */
char*
cstr_dup(const char* str)
{
    char* s2;
    int len = (int)strlen(str);
    s2 = MALLOC(len + 1);
    strcpy(s2, str);
    return s2;
}

/* ------------------------------------------------------------------------- */
void
cstr_free(char* str)
{
    FREE(str);
}

/* ------------------------------------------------------------------------- */
char*
cstr_strip(char* str, char strip)
{
    int len = (int)strlen(str);
    char* end = len > 0 ? str + len - 1 : str;
    while (*str == strip)
        *str++ = '\0';
    while (*end == strip)
        *end-- = '\0';
    return str;
}

/* ------------------------------------------------------------------------- */
void
cstr_split2_strip(char** str1, char** str2, char delimiter, char strip)
{
    char* tmp = strchr(*str1, delimiter);
    if (tmp)
    {
        *tmp++ = '\0';
        *str2 = cstr_strip(tmp, strip);
    }
    else
        *str2 = "";

    *str1 = cstr_strip(*str1, strip);
}

/* ------------------------------------------------------------------------- */
int
string_ends_with(const char* str, const char* end)
{
    int str_len = (int)strlen(str);
    int end_len = (int)strlen(end);
    if (str_len < end_len)
        return 0;
    return strcmp(str + str_len - end_len, end) == 0;
}