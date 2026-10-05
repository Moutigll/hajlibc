/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file spin.c
 * @brief POSIX spinlocks implementation.
 * @Created: 2026/10/03 07:41:48 by Moutig
 * @Updated: 2026/10/03 13:04:11 by Moutig
 *
 * A spinlock is a single int. Locking is a CAS loop with a
 * CPU relax hint; there is no syscall, no blocking, no
 * fairness. POSIX says spinlocks are never shared between
 * processes, so PTHREAD_PROCESS_SHARED is refused with ENOTSUP.
 */

#include <pthread.h>
#include <errno.h>
#include <bits/thread/thread.h>
#include <bits/thread/pthread.h>

int pthread_spin_init(pthread_spinlock_t *lock, int pshared)
{
	if (lock == NULL)
		return (EINVAL);
	if (pshared == PTHREAD_PROCESS_SHARED)
		return (ENOTSUP);
	if (pshared != PTHREAD_PROCESS_PRIVATE)
		return (EINVAL);

	return (0);
}

int pthread_spin_destroy(pthread_spinlock_t *lock)
{
	if (lock == NULL)
		return (EINVAL);
	return (0);
}

int pthread_spin_lock(pthread_spinlock_t *lock)
{
	if (lock == NULL)
		return (EINVAL);
	for (;;) {
		int expected = 0;

		if (__haj_atomic_cas(lock, &expected, 1))
			return (0);
		while (__haj_atomic_load(lock) != 0)
			__haj_cpuRelax();
	}
}

int pthread_spin_trylock(pthread_spinlock_t *lock)
{
	int expected = 0;

	if (lock == NULL)
		return (EINVAL);

	if (__haj_atomic_cas(lock, &expected, 1))
		return (0);
	return (EBUSY);
}

int pthread_spin_unlock(pthread_spinlock_t *lock)
{
	if (lock == NULL)
		return (EINVAL);

	__haj_atomic_store(lock, 0);
	return (0);
}
