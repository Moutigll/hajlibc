/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file rwlock.c
 * @brief POSIX read-write locks implementation.
 * @Created: 2026/10/03 07:53:43 by Moutig
 * @Updated: 2026/10/05 12:48:38 by Moutig
 *
 * The rwlock is a single futex word (state) with the encoding
 * documented in <bits/thread/pthread.h>. Readers increment the
 * low bits; a writer sets the WRITER bit. A blocked writer sets
 * WRWAIT so that new readers gate on it, which prevents writer
 * starvation.
 *
 * Wake strategy (no thundering herd on the common path):
 *
 *   - A reader unlock only wakes when it was the last reader
 *     AND a writer is waiting. In the reader-only workload no
 *     wake syscall is ever issued.
 *
 *   - A writer unlock always wakes all. When there is no
 *     WRWAIT bit, all waiters are readers and they will all
 *     proceed in parallel, so the wake is necessary and not a
 *     thundering herd. When WRWAIT is set the waiters are
 *     mixed (readers that blocked before WRWAIT was set and
 *     writers); the readers will re-sleep on seeing WRWAIT
 *     and one of the writers will take the lock. This is the
 *     only path that may wake more threads than strictly
 *     needed, and it only happens under writer contention.
 *
 * The pshared field is only present when the library is built
 * with HAJ_PTHREAD_PROCESS_SHARED=1. In non-shared mode the
 * struct is just the state word, and HAJ_RWLOCK_IS_SHARED
 * always evaluates to 0.
 *
 * Clock selection:
 *   pthread_rwlock_timedrdlock / timedwrlock use CLOCK_REALTIME.
 *   pthread_rwlock_clockrdlock / clockwrlock take the clock as
 *   an argument and pass FUTEX_CLOCK_REALTIME only for
 *   CLOCK_REALTIME.
 */

#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <bits/thread/thread.h>
#include <bits/thread/pthread.h>

/**
 * @brief Wait on a read-write lock.
 * @param r The read-write lock to wait on.
 * @param expected The expected value of the lock's state.
 * @param clockid Clock to use for the deadline.
 * @param abstime The absolute time to wait until, or NULL.
 * @return 0 on success, or an error code on failure.
 */
static __HAJ_INLINE int rwlockWait(struct _hajThreadRwlock	*r,
								   unsigned int				expected,
								   clockid_t				clockid,
								   const struct timespec	*abstime)
{
	int shared = HAJ_RWLOCK_IS_SHARED(r);
	int rc;

	if (abstime != NULL) {
		rc = __haj_futexWaitBitsetOp((int *)&r->state, (int)expected,
									 abstime, FUTEX_BITSET_MATCH_ANY,
									 shared, clockid);
	} else {
		rc = __haj_futexWaitOp((int *)&r->state, (int)expected, shared);
	}
	if (rc < 0) {
		if (errno == EAGAIN || errno == EINTR)
			return (0);
		return (errno);
	}
	return (0);
}

/**
 * @brief Wake every thread waiting on a read-write lock.
 * @param r The read-write lock to wake up threads on.
 */
static __HAJ_INLINE void rwlockWakeAll(struct _hajThreadRwlock *r)
{
	__haj_futexWakeOp((int *)&r->state, 0x7fffffff, HAJ_RWLOCK_IS_SHARED(r));
}

/**
 * @brief Common implementation for acquiring a read lock.
 * @param r The read-write lock to acquire.
 * @param clockid Clock to use for the deadline.
 * @param abstime The absolute time to wait until, or NULL.
 * @return 0 on success, or an error code on failure.
 */
static int rdlockCommon(struct _hajThreadRwlock *r, clockid_t clockid, const struct timespec *abstime)
{
	for (;;) {
		unsigned int s = __haj_atomic_load(&r->state);

		if (s & (HAJ_RWLOCK_WRITER | HAJ_RWLOCK_WRWAIT)) {
			int rc = rwlockWait(r, s, clockid, abstime);
			if (rc != 0)
				return (rc);
			continue;
		}
		if (__haj_atomic_cas(&r->state, &s, s + 1))
			return (0);
	}
}

/**
 * @brief Common implementation for acquiring a write lock.
 * @param r The read-write lock to acquire.
 * @param clockid Clock to use for the deadline.
 * @param abstime The absolute time to wait until, or NULL.
 * @return 0 on success, or an error code on failure.
 */
static int wrlockCommon(struct _hajThreadRwlock *r, clockid_t clockid, const struct timespec *abstime)
{
	for (;;) {
		unsigned int s = __haj_atomic_load(&r->state);

		if (s == 0 || s == HAJ_RWLOCK_WRWAIT) {
			if (__haj_atomic_cas(&r->state, &s, HAJ_RWLOCK_WRITER))
				return (0);
			continue;
		}
		if (!(s & HAJ_RWLOCK_WRWAIT)) {
			unsigned int ns = s | HAJ_RWLOCK_WRWAIT;

			if (!__haj_atomic_cas(&r->state, &s, ns))
				continue;
			s = ns;
		}
		{
			int rc = rwlockWait(r, s, clockid, abstime);

			if (rc != 0)
				return (rc);
		}
	}
}

int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
{
	struct _hajThreadRwlock *r;
	(void)attr;	/* unused in non-shared mode */

	if (rwlock == NULL)
		return (EINVAL);

	r = HAJ_RWLOCK(rwlock);
	r->state = 0;
#if HAJ_PTHREAD_PROCESS_SHARED
	r->pshared = (attr != NULL)
		? HAJ_RWLOCKATTR_CONST(attr)->pshared
		: PTHREAD_PROCESS_PRIVATE;
#endif
	return (0);
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
	if (rwlock == NULL)
		return (EINVAL);
	/*
	 * POSIX says destroying a held or contended rwlock is
	 * undefined. We do not check, as with mutexes.
	 */
	return (0);
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
	if (rwlock == NULL)
		return (EINVAL);
	return (rdlockCommon(HAJ_RWLOCK(rwlock), CLOCK_REALTIME, NULL));
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock)
{
	struct _hajThreadRwlock	*r;
	unsigned int			s;

	if (rwlock == NULL)
		return (EINVAL);

	r = HAJ_RWLOCK(rwlock);
	s = __haj_atomic_load(&r->state);
	if (s & (HAJ_RWLOCK_WRITER | HAJ_RWLOCK_WRWAIT))
		return (EBUSY);
	if (!__haj_atomic_cas(&r->state, &s, s + 1))
		return (EBUSY);
	return (0);
}

int pthread_rwlock_timedrdlock(pthread_rwlock_t *rwlock, const struct timespec *abstime)
{
	if (rwlock == NULL || abstime == NULL)
		return (EINVAL);
	return (rdlockCommon(HAJ_RWLOCK(rwlock), CLOCK_REALTIME, abstime));
}

int pthread_rwlock_clockrdlock(pthread_rwlock_t *rwlock, clockid_t clockid, const struct timespec *abstime)
{
	if (rwlock == NULL || abstime == NULL)
		return (EINVAL);
	if (clockid != CLOCK_REALTIME && clockid != CLOCK_MONOTONIC)
		return (EINVAL);
	return (rdlockCommon(HAJ_RWLOCK(rwlock), clockid, abstime));
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
	if (rwlock == NULL)
		return (EINVAL);
	return (wrlockCommon(HAJ_RWLOCK(rwlock), CLOCK_REALTIME, NULL));
}

int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock)
{
	struct _hajThreadRwlock	*r;
	unsigned int			s;

	if (rwlock == NULL)
		return (EINVAL);

	r = HAJ_RWLOCK(rwlock);
	s = __haj_atomic_load(&r->state);
	if (s != 0 && s != HAJ_RWLOCK_WRWAIT)
		return (EBUSY);
	if (!__haj_atomic_cas(&r->state, &s, HAJ_RWLOCK_WRITER))
		return (EBUSY);
	return (0);
}

int pthread_rwlock_timedwrlock(pthread_rwlock_t *rwlock, const struct timespec *abstime)
{
	if (rwlock == NULL || abstime == NULL)
		return (EINVAL);
	return (wrlockCommon(HAJ_RWLOCK(rwlock), CLOCK_REALTIME, abstime));
}

int pthread_rwlock_clockwrlock(pthread_rwlock_t *rwlock, clockid_t clockid, const struct timespec *abstime)
{
	if (rwlock == NULL || abstime == NULL)
		return (EINVAL);
	if (clockid != CLOCK_REALTIME && clockid != CLOCK_MONOTONIC)
		return (EINVAL);
	return (wrlockCommon(HAJ_RWLOCK(rwlock), clockid, abstime));
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
	struct _hajThreadRwlock	*r;
	unsigned int			old;
	unsigned int			ns;

	if (rwlock == NULL)
		return (EINVAL);

	r = HAJ_RWLOCK(rwlock);
	old = __haj_atomic_load(&r->state);

	if (old & HAJ_RWLOCK_WRITER) {
		/*
		 * Writer unlock.
		 *
		 * We hold the lock alone, so the state is WRITER,
		 * possibly with WRWAIT set by a writer that blocked
		 * while we held the lock. Use a CAS loop so that a
		 * concurrent CAS that sets WRWAIT is not lost
		 * between our load and our store.
		 */
		do {
			ns = (old & HAJ_RWLOCK_WRWAIT)
				? HAJ_RWLOCK_WRWAIT
				: 0;
		} while (!__haj_atomic_cas(&r->state, &old, ns));

		/*
		 * Wake every waiter. When ns == 0 they are all
		 * readers and they will enter the lock in parallel.
		 * When ns == WRWAIT the waiters are mixed and one
		 * of the writers will take the lock.
		 */
		rwlockWakeAll(r);
		return (0);
	}

	/*
	 * Reader unlock: decrement the reader count. If we were
	 * the last reader and a writer is waiting, wake every
	 * waiter so that one of the writers can proceed.
	 */
	__haj_atomic_sub_fetch(&r->state, 1);
	if (__haj_atomic_load(&r->state) == HAJ_RWLOCK_WRWAIT)
		rwlockWakeAll(r);
	return (0);
}
