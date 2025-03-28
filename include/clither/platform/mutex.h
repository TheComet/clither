#pragma once

struct mutex;

struct mutex*
mutex_create(void);

struct mutex*
mutex_create_recursive(void);

void
mutex_destroy(struct mutex* m);

void
mutex_lock(struct mutex* m);

/*!
 * \brief Attempts to lock a mutex.
 * \param[in] m Mutex
 * \return Returns non-zero if the lock was acquired. Zero if the lock was not
 * acquired.
 */
int
mutex_trylock(struct mutex* m);

void
mutex_unlock(struct mutex* m);
