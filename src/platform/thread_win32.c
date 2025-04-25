#include "clither/util/log.h"
#include "clither/platform/thread.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct thread*
thread_start(void* (*func)(const void*), const void* args)
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
        log_err_win32("Failed to create thread\n");
        return NULL;
    }

    return (struct thread*)hThread;
}

void*
thread_join(struct thread* t)
{
    DWORD ret;
    HANDLE hThread = (HANDLE)t;
    if (WaitForSingleObject(hThread, INFINITE) != 0)
    {
        log_err_win32("WaitForSingleObject failed in thread_join()\n");
        return (void*)-1;
    }

    GetExitCodeThread(hThread, &ret);
    CloseHandle(hThread);
    return (void*)(intptr_t)ret;
}

void
thread_kill(struct thread* t)
{
    HANDLE hThread = (HANDLE)t;
    if (TerminateThread(hThread, (DWORD)-1) == FALSE)
        log_err_win32("Failed to TerminateThread()\n");
    CloseHandle(hThread);
}

void
thread_sigint(struct thread* t)
{
    HANDLE hThread = (HANDLE)t;
    if (TerminateThread(hThread, (DWORD)-1) == FALSE)
        log_err_win32("Failed to TerminateThread()\n");
}
