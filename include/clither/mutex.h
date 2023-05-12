#pragma once

struct mutex
{
    void* handle;
};

void
mutex_init(struct mutex* m);

void
mutex_init_recursive(struct mutex* m);

void
mutex_deinit(struct mutex m);

void
mutex_lock(struct mutex m);

int
mutex_trylock(struct mutex m);

void
mutex_unlock(struct mutex m);
