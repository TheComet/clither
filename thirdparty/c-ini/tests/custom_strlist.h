#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

struct strlist;
int  custom_strlist_init(struct strlist** l);
void custom_strlist_deinit(struct strlist* l);
int  custom_strlist_add(struct strlist** l, const char* data, int len);
void custom_strlist_clear(struct strlist* l);

#if defined(__cplusplus)
}
#endif
