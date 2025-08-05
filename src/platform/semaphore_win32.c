#include "clither/platform/mutex.h"
#include "clither/util/tracker.h"
#include "clither/util/log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ------------------------------------------------------------------------- */
struct semaphore* semaphore_create(int initial_count)
{
    HANDLE hSem = CreateSemaphore(NULL, initial_count, LONG_MAX, NULL);
    if (hSem == NULL)
    {
        log_err_win32("Failed to create semaphore\n");
        return NULL;
    }

    track_fd((int)(intptr_t)hSem);
    return (struct semaphore*)hSem;
}

/* ------------------------------------------------------------------------- */
void semaphore_destroy(struct semaphore* sem)
{
    untrack_fd((int)(intptr_t)sem);
    CloseHandle((HANDLE)sem);
}

/* ------------------------------------------------------------------------- */
void semaphore_take(struct semaphore* sem)
{
    while (WaitForSingleObject((HANDLE)sem, INFINITE) != WAIT_OBJECT_0)
        ;
}

/* ------------------------------------------------------------------------- */
void semaphore_give(struct semaphore* sem)
{
    ReleaseSemaphore((HANDLE)sem, 1, NULL);
}

/* ------------------------------------------------------------------------- */
int semaphore_try_take(struct semaphore* sem)
{
    return WaitForSingleObject((HANDLE)sem, 0) == WAIT_OBJECT_0;
}
