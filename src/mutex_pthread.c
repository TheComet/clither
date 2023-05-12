#include "clither/mutex.h"

#include <pthread.h>
#include <stdlib.h>

/* ------------------------------------------------------------------------- */
void
mutex_init(struct mutex* m)
{
    pthread_mutexattr_t attr;
    m->handle = malloc(sizeof(pthread_mutex_t));
    pthread_mutexattr_init(&attr);
    pthread_mutex_init(m->handle, &attr);
    pthread_mutexattr_destroy(&attr);
}

/* ------------------------------------------------------------------------- */
void
mutex_init_recursive(struct mutex* m)
{
    pthread_mutexattr_t attr;
    m->handle = malloc(sizeof(pthread_mutex_t));
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m->handle, &attr);
    pthread_mutexattr_destroy(&attr);
}

/* ------------------------------------------------------------------------- */
void
mutex_deinit(struct mutex m)
{
    pthread_mutex_destroy(m.handle);
    free(m.handle);
}

/* ------------------------------------------------------------------------- */
void
mutex_lock(struct mutex m)
{
    pthread_mutex_lock(m.handle);
}

/* ------------------------------------------------------------------------- */
int
mutex_trylock(struct mutex m)
{
    return pthread_mutex_trylock(m.handle) == 0;
}

/* ------------------------------------------------------------------------- */
void
mutex_unlock(struct mutex m)
{
    pthread_mutex_unlock(m.handle);
}
