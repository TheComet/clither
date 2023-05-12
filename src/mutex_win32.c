#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "clither/log.h"
#include "clither/mutex.h"

#include <stdlib.h>

/* ------------------------------------------------------------------------- */
void
mutex_init(struct mutex* m)
{
    m->handle = malloc(sizeof(CRITICAL_SECTION));
    InitializeCriticalSectionAndSpinCount(m->handle, 0x00000400);
}

/* ------------------------------------------------------------------------- */
void
mutex_init_recursive(struct mutex* m)
{
    m->handle = malloc(sizeof(CRITICAL_SECTION));
    InitializeCriticalSectionAndSpinCount(m->handle, 0x00000400);
}

/* ------------------------------------------------------------------------- */
void
mutex_deinit(struct mutex m)
{
    DeleteCriticalSection(m.handle);
    free(m.handle);
}

/* ------------------------------------------------------------------------- */
void
mutex_lock(struct mutex m)
{
    EnterCriticalSection(m.handle);
}

/* ------------------------------------------------------------------------- */
int
mutex_trylock(struct mutex m)
{
    return TryEnterCriticalSection(m.handle);
}

/* ------------------------------------------------------------------------- */
void
mutex_unlock(struct mutex m)
{
    LeaveCriticalSection(m.handle);
}
