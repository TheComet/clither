#pragma once

struct semaphore;

struct semaphore* semaphore_create(int initial_count);
void              semaphore_destroy(struct semaphore* sem);

void semaphore_take(struct semaphore* sem);
void semaphore_give(struct semaphore* sem);
int semaphore_try_take(struct semaphore* sem);
