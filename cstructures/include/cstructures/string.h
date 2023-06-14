#pragma once

#include "cstructures/config.h"
#include "cstructures/vector.h"
#include <stdint.h>
#include <stdio.h>

C_BEGIN

struct cs_string
{
    struct cs_vector buf;
};

struct cs_string_split_state
{
    char token;
    size_t split_count;
};

CSTRUCTURES_PUBLIC_API struct cs_string*
string_create(void);

CSTRUCTURES_PUBLIC_API void
string_init(struct cs_string* str);

CSTRUCTURES_PUBLIC_API void
string_deinit(struct cs_string* str);

CSTRUCTURES_PUBLIC_API void
string_destroy(struct cs_string* str);

CSTRUCTURES_PUBLIC_API void
string_set(struct cs_string* str, const char* c);

CSTRUCTURES_PUBLIC_API struct cs_string*
string_cat(struct cs_string* str, const char* c_str);

CSTRUCTURES_PUBLIC_API struct cs_string*
string_cat2(struct cs_string* str, const char* s1, const char* s2);

CSTRUCTURES_PUBLIC_API struct cs_string*
string_join(struct cs_string* str, int n, ...);

CSTRUCTURES_PUBLIC_API char*
string_take(struct cs_string* str);

/*!
 * @brief Reads a line of text from the stream.
 * @note Empty lines are ignored, i.e. this function is guaranteed to return
 * a non-zero length string.
 * @return Returns 1 if a new string was read successfully. Returns 0 if there
 * are no more strings to read. Returns -1 if an error occurred.
 */
CSTRUCTURES_PUBLIC_API int
string_getline(struct cs_string* str, FILE* fp);

char*
string_tok(struct cs_string* str, char delimiter, char** saveptr);

char*
string_tok_strip(struct cs_string* str, char delimiter, char strip, char** saveptr);

char*
string_tok_strip_c(char* str, char delimiter, char strip, char** saveptr);

char*
cstr_dup(const char* str);

void
cstr_free(char* str);

/*!
 * \brief Removes all occurrences of "strip" from the beginning and end of
 * the string. This function operates in-place and modifies the buffer.
 */
char*
cstr_strip(char* str, char strip);

void
cstr_split2_strip(char** str1, char** str2, char delimiter, char strip);

int
string_ends_with(const char* str, const char* end);

#define string_length(str) \
        (vector_count(&(str)->buf) - 1)

#define string_cstr(str) \
        ((char*)vector_data(&(str)->buf))

C_END
