/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file mutex.c
 * @brief POSIX mutex implementation.
 * @Created: 2026/10/02 12:11:16 by Moutig
 * @Updated: 2026/10/02 15:20:22 by Moutig
 *
 * The mutex is a single futex word (the `lock` field) plus a
 * few bookkeeping fields (owner TID, count, type).
 *
 * Fast path (uncontended):
 *   lock CAS 0 -> 1 succeeds, no syscall.
 *   unlock: if count is 0, atomic store lock = 0.
 *
 * Slow path (contended):
 *   lock CAS fails, we spin a few times, then call
 *   FUTEX_WAIT on the futex word.
 *   A thread that went through the slow path always takes the
 *   lock in the CONTENDED state (0 -> 2), because it cannot know
 *   whether other waiters are still sleeping. Otherwise its
 *   unlock would not wake them (lost wakeup -> deadlock).
 *   unlock: atomic exchange lock 1 -> 0; if the previous value
 *   was 2 (contended), call FUTEX_WAKE.
 *
 * Owner tracking is what makes RECURSIVE and ERRORCHECK
 * possible, and lets us return EPERM on unlock by a non-owner.
 * For NORMAL mutexes, the owner field is still set so that
 * pthread_mutex_consistent can know who owns it, but no error
 * is checked.
 */

#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <bits/thread/thread.h>
#include <bits/thread/pthread.h>
#include <bits/thread/tcb.h>


/*
 * The futex word has three states:
 *   0  free
 *   1  locked, no waiter
 *   2  locked, at least one waiter (so the unlocker must wake)
 */
# define HAJ_MUTEX_FREE		0
# define HAJ_MUTEX_LOCKED	1
# define HAJ_MUTEX_CONTENDED	2

/* Number of spin iterations before falling back to the futex. */
# define HAJ_MUTEX_SPIN		100

/**
 * @brief Try to acquire a mutex.
 * @param m The mutex to acquire.
 * @return 1 if the mutex was acquired, 0 otherwise.
 */
static int tryAcquire(struct _hajThreadMutex *m)
{
	int	expected = HAJ_MUTEX_FREE;

	return (__haj_atomic_cas(&m->lock, &expected, HAJ_MUTEX_LOCKED));
}

/**
 * @brief Try to acquire a mutex in the CONTENDED state.
 *
 * Used by the slow path only: a thread that may have slept must
 * take the lock as CONTENDED so that its unlock wakes the other
 * sleeping waiters, if any.
 * @param m The mutex to acquire.
 * @return 1 if the mutex was acquired, 0 otherwise.
 */
static int tryAcquireContended(struct _hajThreadMutex *m)
{
	int	expected = HAJ_MUTEX_FREE;

	return (__haj_atomic_cas(&m->lock, &expected, HAJ_MUTEX_CONTENDED));
}

/**
 * @brief Wait on a mutex futex.
 *
 * This function waits on the mutex's futex word until it is
 * released or the absolute timeout is reached. If the mutex is
 * already free, it returns immediately.
 * The futex word is expected to be CONTENDED: that is the value
 * the callers have just written, and FUTEX_WAIT with any other
 * value would return EAGAIN right away (busy loop).
 * @param m The mutex to wait on.
 * @param abstime Absolute timeout, or NULL for infinite wait.
 * @return 0 on success, or an errno value on failure.
 */
static int waitFutex(struct _hajThreadMutex *m, const struct timespec *abstime)
{
	int	expected;

	expected = __haj_atomic_load(&m->lock);
	if (expected == HAJ_MUTEX_FREE)
		return (0);	/* someone released it, retry */

	if (abstime != NULL) {
		int r = __haj_futexWaitBitset(&m->lock,
									  HAJ_MUTEX_CONTENDED,
									  abstime,
									  FUTEX_BITSET_MATCH_ANY);
		if (r < 0) {
			if (errno == EAGAIN || errno == EINTR)
				return (0);	/* value changed or signal, retry */
			return (errno);
		}
		return (0);
	}

	if (__haj_futexWait(&m->lock, HAJ_MUTEX_CONTENDED) < 0) {
		if (errno == EAGAIN)
			return (0);	/* value changed, retry */
		if (errno == EINTR)
			return (0);	/* signal, retry */
		return (errno);
	}
	return (0);
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	struct _hajThreadMutex				*m;
	const struct _hajThreadMutexAttr	*a;

	if (mutex == NULL)
		return (EINVAL);

	m = HAJ_MUTEX(mutex);
	m->lock		= HAJ_MUTEX_FREE;
	m->owner	= 0;
	m->count	= 0;

	if (attr != NULL) {
		a = HAJ_MUTEXATTR_CONST(attr);
		m->type = a->type;
		m->robust = a->robust;
	} else {
		m->type = PTHREAD_MUTEX_DEFAULT;
		m->robust = PTHREAD_MUTEX_STALLED;
	}

	return (0);
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	struct _hajThreadMutex	*m;

	if (mutex == NULL)
		return (EINVAL);

	m = HAJ_MUTEX(mutex);

	/*
	 * POSIX says destroying a locked or contended mutex is
	 * undefined. We do not check, to keep the fast path fast.
	 * A well-behaved program never destroys a locked mutex.
	 */
	(void)m;
	return (0);
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	struct _hajThreadMutex	*m;
	int		tid;
	int		r;

	if (mutex == NULL)
		return (EINVAL);

	m = HAJ_MUTEX(mutex);
	tid = __haj_gettid();

	/* Fast path: uncontended CAS. */
	if (tryAcquire(m)) {
		m->owner = tid;
		m->count = 1;
		return (0);
	}

	/* Recursive: same thread already owns it. */
	if (m->type == PTHREAD_MUTEX_RECURSIVE && m->owner == tid) {
		m->count++;
		return (0);
	}

	/* Errorcheck: same thread tries to lock twice. */
	if (m->type == PTHREAD_MUTEX_ERRORCHECK && m->owner == tid)
		return (EDEADLK);

	/* Slow path: contended. */
	for (;;) {
		int v = __haj_atomic_load(&m->lock);
		if (v == HAJ_MUTEX_FREE) {
			if (tryAcquireContended(m))
				break;
			continue;
		}
		/*
		 * Promote LOCKED to CONTENDED so the unlocker knows
		 * it must wake us. If it was already CONTENDED,
		 * the CAS fails but that is fine.
		 */
		if (v == HAJ_MUTEX_LOCKED)
			(void)__haj_atomic_cas(&m->lock, &v, HAJ_MUTEX_CONTENDED);
		r = waitFutex(m, NULL);
		if (r != 0)
			return (r);
	}
	m->owner = tid;
	m->count = 1;
	return (0);
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	struct _hajThreadMutex	*m;
	int		tid;

	if (mutex == NULL)
		return (EINVAL);

	m = HAJ_MUTEX(mutex);
	tid = __haj_gettid();

	if (tryAcquire(m)) {
		m->owner = tid;
		m->count = 1;
		return (0);
	}

	if (m->type == PTHREAD_MUTEX_RECURSIVE && m->owner == tid) {
		m->count++;
		return (0);
	}

	if (m->type == PTHREAD_MUTEX_ERRORCHECK && m->owner == tid)
		return (EDEADLK);

	return (EBUSY);
}

int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime)
{
	struct _hajThreadMutex	*m;
	int		tid;
	int		r;

	if (mutex == NULL)
		return (EINVAL);

	m = HAJ_MUTEX(mutex);
	tid = __haj_gettid();

	if (tryAcquire(m)) {
		m->owner = tid;
		m->count = 1;
		return (0);
	}

	if (m->type == PTHREAD_MUTEX_RECURSIVE && m->owner == tid) {
		m->count++;
		return (0);
	}

	if (m->type == PTHREAD_MUTEX_ERRORCHECK && m->owner == tid)
		return (EDEADLK);

	for (;;) {
		int v = __haj_atomic_load(&m->lock);
		if (v == HAJ_MUTEX_FREE) {
			if (tryAcquireContended(m))
				break;
			continue;
		}
		if (v == HAJ_MUTEX_LOCKED)
			(void)__haj_atomic_cas(&m->lock, &v, HAJ_MUTEX_CONTENDED);
		r = waitFutex(m, abstime);
		if (r != 0)
			return (r);
	}

	m->owner = tid;
	m->count = 1;
	return (0);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	struct _hajThreadMutex	*m;
	int		tid;
	int		prev;

	if (mutex == NULL)
		return (EINVAL);

	m = HAJ_MUTEX(mutex);
	tid = __haj_gettid();

	/* Errorcheck / recursive: not the owner. */
	if ((m->type == PTHREAD_MUTEX_ERRORCHECK
		|| m->type == PTHREAD_MUTEX_RECURSIVE) && m->owner != tid)
		return (EPERM);

	/* Recursive: decrement the count. */
	if (m->type == PTHREAD_MUTEX_RECURSIVE && m->owner == tid) {
		m->count--;
		if (m->count > 0)
			return (0);
	}

	m->owner = 0;
	m->count = 0;

	/*
	 * Release. Use exchange so we can tell if there was a
	 * waiter: if the previous value was CONTENDED, wake one.
	 */
	prev = __haj_atomic_exchange(&m->lock, HAJ_MUTEX_FREE);
	if (prev == HAJ_MUTEX_CONTENDED)
		__haj_futexWake(&m->lock, 1);

	return (0);
}
