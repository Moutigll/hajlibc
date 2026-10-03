/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file barrier.c
 * @brief POSIX barriers implementation.
 * @Created: 2026/10/03 07:48:48 by Moutig
 * @Updated: 2026/10/03 13:44:27 by Moutig
 *
 * A barrier is a futex word (seq) plus two counters: total and
 * count. Each arriving thread increments count. The thread
 * whose fetch_add returns total-1 is the last one: it resets
 * count, bumps seq and wakes everyone. The other threads block
 * on seq until it differs from the value they saw when they
 * arrived.
 *
 * The pshared field is only present when the library is built
 * with HAJ_PTHREAD_PROCESS_SHARED=1. In non-shared mode the
 * struct is just total + count + seq, and HAJ_BARRIER_IS_SHARED
 * always evaluates to 0.
 */

#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <bits/thread/thread.h>
#include <bits/thread/pthread.h>

int pthread_barrier_init(pthread_barrier_t				*barrier,
						 const pthread_barrierattr_t	*attr,
						 unsigned int					count)
{
	struct _hajThreadBarrier *b;
	(void)attr;	/* unused in non-shared mode */

	if (barrier == NULL || count == 0)
		return (EINVAL);

	b = HAJ_BARRIER(barrier);
	b->total = count;
	b->count = 0;
	b->seq = 0;
#if HAJ_PTHREAD_PROCESS_SHARED
	b->pshared = (attr != NULL)
		? HAJ_BARRIERATTR_CONST(attr)->pshared
		: PTHREAD_PROCESS_PRIVATE;
#endif
	return (0);
}

int pthread_barrier_destroy(pthread_barrier_t *barrier)
{
	if (barrier == NULL)
		return (EINVAL);
	/*
	 * POSIX says destroying a barrier on which threads are
	 * blocked is undefined. We do not check, as with mutexes
	 * and rwlocks.
	 */
	return (0);
}

int pthread_barrier_wait(pthread_barrier_t *barrier)
{
	struct _hajThreadBarrier	*b;
	int							shared;
	int							mySeq;
	int							old;

	if (barrier == NULL)
		return (EINVAL);

	b = HAJ_BARRIER(barrier);
	shared = HAJ_BARRIER_IS_SHARED(b);
	mySeq = __haj_atomic_load(&b->seq);

	old = __haj_atomic_add_fetch(&b->count, 1);
	if (old == (int)b->total) {
		/*
		 * Last thread to arrive. Reset the count for the
		 * next generation, bump seq so the waiters see a
		 * change, then wake them all.
		 */
		__haj_atomic_store(&b->count, 0);
		__haj_atomic_add_fetch(&b->seq, 1);
		__haj_futexWakeOp(&b->seq, 0x7fffffff, shared);
		return (PTHREAD_BARRIER_SERIAL_THREAD);
	}

	/*
	 * Not the last: block until seq changes. A spurious
	 * wakeup or a signal makes the futex return EAGAIN or
	 * EINTR; in both cases we re-check the condition.
	 */
	while (__haj_atomic_load(&b->seq) == mySeq) {
		if (__haj_futexWaitOp(&b->seq, mySeq, shared) < 0
			&& errno != EAGAIN && errno != EINTR)
			return (errno);
	}
	return (0);
}

/* ----- Barrier Attributes ----- */

int pthread_barrierattr_init(pthread_barrierattr_t *attr)
{
	struct _hajThreadBarrierAttr *a;

	if (attr == NULL)
		return (EINVAL);

	memset(attr, 0, sizeof(*attr));
	a = HAJ_BARRIERATTR(attr);
	a->pshared = PTHREAD_PROCESS_PRIVATE;
	return (0);
}

int pthread_barrierattr_destroy(pthread_barrierattr_t *attr)
{
	if (attr == NULL)
		return (EINVAL);
	return (0);
}

int pthread_barrierattr_setpshared(pthread_barrierattr_t *attr, int pshared)
{
	struct _hajThreadBarrierAttr *a;

	if (attr == NULL)
		return (EINVAL);
	if (pshared != PTHREAD_PROCESS_PRIVATE
		&& pshared != PTHREAD_PROCESS_SHARED)
		return (EINVAL);
#if !HAJ_PTHREAD_PROCESS_SHARED
	/*
	 * The library was built without process-shared support.
	 * POSIX explicitly allows returning ENOTSUP in this case.
	 */
	if (pshared == PTHREAD_PROCESS_SHARED)
		return (ENOTSUP);
#endif

	a = HAJ_BARRIERATTR(attr);
	a->pshared = pshared;
	return (0);
}

int pthread_barrierattr_getpshared(const pthread_barrierattr_t *attr, int *pshared)
{
	if (attr == NULL || pshared == NULL)
		return (EINVAL);
	*pshared = HAJ_BARRIERATTR_CONST(attr)->pshared;
	return (0);
}
