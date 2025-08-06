#pragma once

#include "clither/config.h"

#if defined(CLITHER_DEBUG_MEMORY)

/* Generic tracker API used by all memory/resource tracking. */
struct tracker;
struct tracker* tracker_create(const char* name);
void            tracker_destroy(struct tracker* t);
void tracker_track(struct tracker* t, void* p, int size, const char* name);
void tracker_untrack(struct tracker* t, void* p);

/* Specific trackers for different resources. These are accessed globally
 * (stored per thread). The reason is so other API function signatures don't
 * change when resource tracking is didsabled. */
int  trackers_init_tls(void);
void trackers_deinit_tls(void);

void track_mem(void* p, int size, const char* name);
void untrack_mem(void* p);

void track_fd(int fd, const char* name);
void untrack_fd(int fd);

#else

/* clang-format off */
#   define trackers_init_tls()    (0)
#   define trackers_deinit_tls()  do {} while (0)
#   define track_mem(p, size)     do {} while (0)
#   define untrack_mem(p)         do {} while (0)
#   define track_fd(fd)           do {} while (0)
#   define untrack_fd(fd)         do {} while (0)
/* clang-format on */

#endif
