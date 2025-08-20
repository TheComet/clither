#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

struct str;
int         custom_str_init(struct str** str);
void        custom_str_deinit(struct str* str);
int         custom_str_set(struct str** str, const char* data, int len);
const char* custom_str_data(const struct str* str);
int         custom_str_len(const struct str* str);

#if defined(__cplusplus)
}
#endif
