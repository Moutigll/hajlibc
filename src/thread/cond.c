/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file cond.c
 * @brief Implementation of pthread_cond_*().
 * @Created: 2026/10/02 14:29:28 by Moutig
 * @Updated: 2026/10/03 08:52:10 by Moutig
 *
 * A condition variable is a single futex word: a generation
 * counter (`seq`). Each signal or broadcast increments it and
 * wakes the appropriate number of waiters.
 *
 * pthread_cond_wait does:
 *   1. Read seq.
 *   2. Unlock the mutex.
 *   3. FUTEX_WAIT on seq with the value read at step 1.
 *   4. Re-lock the mutex.
 *
 * Because the value read at step 1 is passed to FUTEX_WAIT, a
 * signal arriving between step 1 and step 3 will change seq
 * and cause FUTEX_WAIT to return EAGAIN immediately. No lost
 * wakeup, no need to remember the signal.
 *
 * pthread_cond_signal increments seq and wakes one waiter.
 * pthread_cond_broadcast increments seq and wakes all waiters.
 *
 * The clock used by timedwait is stored in the cond itself, so
 * that pthread_cond_timedwait (which does not take a clockid)
 * can use the one selected by pthread_condattr_setclock at
 * init time. pthread_cond_clockwait uses an explicit clockid
 * and ignores the stored one.
 */

#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <bits/thread/thread.h>
#include <bits/thread/pthread.h>

/* ----- Internal helpers ----- */

/*
 * Wait on the cond's seq word until it changes from `seq` or
 * the absolute timeout expires. The mutex is not touched: the
 * caller manages it.
 *
 * The futex operation is composed explicitly:
 *
 *   FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG  always
 *   FUTEX_CLOCK_REALTIME                    only when using
 *                                           CLOCK_REALTIME
 *
 * The kernel interprets the absence of FUTEX_CLOCK_REALTIME as
 * "use the monotonic clock for the timeout". So we set the
 * flag only for REALTIME, and leave it off for MONOTONIC.
 *
 * EINTR is retried internally. POSIX requires wait to return
 * only on signal (real signal, not EINTR from the kernel),
 * timeout, or error. Retrying is correct because abstime is
 * absolute: the kernel recomputes the remaining time on each
 * call.
 *
 * Returns:
 *   0          seq changed (signal, broadcast, or spurious)
 *   ETIMEDOUT  on timeout
 *   EINVAL     invalid clockid or bad uaddr
 *   other      errno value
 */
static int waitSeq(struct _hajThreadCond *c, int seq, clockid_t clockid, const struct timespec *abstime)
{
	int		op;
	long	r;

	op = HAJ_FUTEX_OP_WAIT_BITSET(HAJ_COND_IS_SHARED(c));
	if (clockid == CLOCK_REALTIME)
		op |= FUTEX_CLOCK_REALTIME;

	for (;;) {
		r = __haj_futex(&c->seq, op, seq, abstime, NULL, FUTEX_BITSET_MATCH_ANY);
		if (r == 0)
			return (0);
		if (errno == EAGAIN)
			return (0);	/* seq changed, no need to wait */
		if (errno == EINTR)
			continue;	/* signal, retry with same abstime */
		if (errno == ETIMEDOUT)
			return (ETIMEDOUT);
		return (errno);
	}
}



int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
	struct _hajThreadCond	*c;

	if (cond == NULL)
		return (EINVAL);

	c = HAJ_COND(cond);
	c->seq = 0;
	c->clock = (attr != NULL)
		? HAJ_CONDATTR_CONST(attr)->clock
		: CLOCK_REALTIME;
#if HAJ_PTHREAD_PROCESS_SHARED
	c->pshared = (attr != NULL)
		? HAJ_CONDATTR_CONST(attr)->pshared
		: PTHREAD_PROCESS_PRIVATE;
#endif

	return (0);
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	if (cond == NULL)
		return (EINVAL);

	/*
	 * POSIX says destroying a condition variable that still
	 * has waiters is undefined. We do not check, to keep
	 * the fast path fast. A well-behaved program never does
	 * this.
	 */
	return (0);
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	struct _hajThreadCond	*c;
	int	seq;

	if (cond == NULL || mutex == NULL)
		return (EINVAL);

	c = HAJ_COND(cond);

	/*
	 * Read seq BEFORE releasing the mutex. If we read after,
	 * a signal could slip between the unlock and the read,
	 * and we would sleep on the new value, which would
	 * never be signaled again.
	 */
	seq = __haj_atomic_load(&c->seq);

	pthread_mutex_unlock(mutex);

	/*
	 * No timeout. waitSeq never returns ETIMEDOUT when
	 * abstime is NULL, and EINTR is retried internally, so
	 * the only errors possible are EINVAL (bad uaddr) or
	 * an unexpected errno. All are reported as 0 here: POSIX
	 * pthread_cond_wait returns 0 on success and does not
	 * define a specific error return for spurious wakeups
	 * or signals.
	 */
	(void)waitSeq(c, seq, CLOCK_REALTIME, NULL);

	/*
	 * Re-acquire the mutex. POSIX requires the mutex to be
	 * held on return, no matter what happened during the
	 * wait.
	 */
	pthread_mutex_lock(mutex);

	return (0);
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
{
	struct _hajThreadCond	*c;
	int	seq;
	int	r;

	if (cond == NULL || mutex == NULL || abstime == NULL)
		return (EINVAL);

	c = HAJ_COND(cond);

	seq = __haj_atomic_load(&c->seq);

	pthread_mutex_unlock(mutex);

	r = waitSeq(c, seq, c->clock, abstime);

	pthread_mutex_lock(mutex);

	return (r);
}

int pthread_cond_clockwait(pthread_cond_t *cond, pthread_mutex_t *mutex, clockid_t clockid, const struct timespec *abstime)
{
	struct _hajThreadCond	*c;
	int	seq;
	int	r;

	if (cond == NULL || mutex == NULL || abstime == NULL)
		return (EINVAL);
	if (clockid != CLOCK_REALTIME && clockid != CLOCK_MONOTONIC)
		return (EINVAL);

	c = HAJ_COND(cond);

	seq = __haj_atomic_load(&c->seq);

	pthread_mutex_unlock(mutex);

	r = waitSeq(c, seq, clockid, abstime);

	pthread_mutex_lock(mutex);

	return (r);
}

int pthread_cond_signal(pthread_cond_t *cond)
{
	struct _hajThreadCond	*c;

	if (cond == NULL)
		return (EINVAL);

	c = HAJ_COND(cond);

	/*
	 * Increment the generation counter, then wake one
	 * waiter. The order does not matter for correctness:
	 * a waiter that has not yet slept will see the new
	 * value and skip the wait; a waiter that is sleeping
	 * will be woken and re-check.
	 */
	__haj_atomic_add_fetch(&c->seq, 1);
	__haj_futexWakeOp(&c->seq, 1, HAJ_COND_IS_SHARED(c));
	return (0);
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
	struct _hajThreadCond	*c;

	if (cond == NULL)
		return (EINVAL);

	c = HAJ_COND(cond);

	__haj_atomic_add_fetch(&c->seq, 1);
	__haj_futexWakeOp(&c->seq, 0x7fffffff, HAJ_COND_IS_SHARED(c));
	return (0);
}
