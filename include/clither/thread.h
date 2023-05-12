#pragma once

struct thread
{
    void* handle;
};

int
thread_start(struct thread* t, void* (*func)(void*), const void* args);

int
thread_join(struct thread t, int timeout_ms);

void
thread_kill(struct thread t);
