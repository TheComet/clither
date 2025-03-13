#pragma once

struct thread;

/*! Calls func in a new thread. Returns NULL on failure. */
struct thread* thread_start(void* (*func)(const void*), const void* args);

/*! Returns the return value of the thread, or (void*)-1 on failure. */
void* thread_join(struct thread* t);

void thread_kill(struct thread* t);
