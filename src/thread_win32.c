#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "clither/log.h"
#include "clither/thread.h"

/* ------------------------------------------------------------------------- */
int
thread_start(struct thread* t, void* (*func)(void*), const void* args)
{
    HANDLE hThread = CreateThread(
        NULL,  /* Security attributes*/
        0,     /* Initial stack size */
        (LPTHREAD_START_ROUTINE)func,
        (void*)args,
        0,     /* Run thread immediately */
        NULL); /* tid */
    if (hThread == NULL)
    {
        log_err("Failed to create thread\n");
        return -1;
    }

    t->handle = (void*)hThread;
    return 0;
}

/* ------------------------------------------------------------------------- */
int
thread_join(struct thread t, int timeout_ms)
{
    HANDLE hThread = (HANDLE)t.handle;
    switch (WaitForSingleObject(hThread, timeout_ms == 0 ? INFINITE : (DWORD)timeout_ms))
    {
    case WAIT_FAILED:
        log_err("WaitForSingleObject failed in thread_join(): %d\n", GetLastError());
        /* fallthrough */
    case WAIT_ABANDONED:
    case WAIT_TIMEOUT:
        return -1;

    default:
        break;
    }
    CloseHandle(hThread);
    return 0;
}

/* ------------------------------------------------------------------------- */
void
thread_kill(struct thread t)
{
    HANDLE hThread = (HANDLE)t.handle;
    if (TerminateThread(hThread, (DWORD)-1) == FALSE)
        log_err("Failed to TerminateThread: %d\n", GetLastError());
    CloseHandle(hThread);
}
