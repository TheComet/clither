#include "clither/util/mem.h"
#include <errno.h>
#include <semaphore.h>
#include <stddef.h>

struct semaphore
{
    sem_t handle;
};

/* ------------------------------------------------------------------------- */
struct semaphore* semaphore_create(int initial_count)
{
    struct semaphore* sem;
    int               rc;

    sem = mem_alloc(sizeof(*sem));
    if (sem == NULL)
        goto alloc_sem_failed;

    rc = sem_init(&sem->handle, 0, initial_count);
    if (rc != 0)
        goto init_sem_failed;

    return sem;

init_sem_failed:
    mem_free(sem);
alloc_sem_failed:
    return NULL;
}

/* ------------------------------------------------------------------------- */
void semaphore_destroy(struct semaphore* sem)
{
    sem_destroy(&sem->handle);
    mem_free(sem);
}

/* ------------------------------------------------------------------------- */
void semaphore_take(struct semaphore* sem)
{
    int rc;

    do
    {
        rc = sem_wait(&sem->handle);
    } while (rc == -1 && errno == EINTR);
}

/* ------------------------------------------------------------------------- */
void semaphore_give(struct semaphore* sem)
{
    sem_post(&sem->handle);
}

/* ------------------------------------------------------------------------- */
int semaphore_try_take(struct semaphore* sem)
{
    int rc;

    do
    {
        rc = sem_trywait(&sem->handle);
    } while (rc == -1 && errno == EINTR);

    return rc == 0;
}
