#include "clither/thread.h"
#include "clither/log.h"

#define _GNU_SOURCE
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <signal.h>

/* ------------------------------------------------------------------------- */
int
thread_start(struct thread* t, void* (*func)(const void*), const void* args)
{
    int rc;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if ((rc = pthread_create((pthread_t*)&t->handle, &attr, (void*(*)(void*))func, (void*)args)) != 0)
    {
        pthread_attr_destroy(&attr);
        log_err("Failed to create thread: %s\n", strerror(rc));
        return -1;
    }

    pthread_attr_destroy(&attr);
    return 0;
}

/* ------------------------------------------------------------------------- */
int
thread_join(struct thread t, int timeout_ms)
{
    void* ret;
    struct timespec ts;
    struct timespec off;

    clock_gettime(CLOCK_REALTIME, &ts);
    off.tv_sec = timeout_ms / 1000;
    off.tv_nsec = (timeout_ms - ts.tv_sec * 1000) * 1e6;
    ts.tv_nsec += off.tv_nsec;
    while (ts.tv_nsec >= 1e9)
    {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1e9;
    }
    ts.tv_sec += off.tv_sec;

    if (pthread_timedjoin_np((pthread_t)t.handle, &ret, &ts) == 0)
        return 0;
    return -1;
}

/* ------------------------------------------------------------------------- */
void
thread_kill(struct thread t)
{
    pthread_kill((pthread_t)t.handle, SIGKILL);
}
