#pragma once

#include "clither/config.h"

#if !defined(CLITHER_DEBUG_MEMORY)
#else
struct tracker* tracker_create(void);
int             tracker_destroy(struct tracker* t);

void tracker_track(struct tracker* t, void* p, int size);
int  tracker_untrack(struct tracker* t, void* p);
#endif
