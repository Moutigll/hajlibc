/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file lock.c
 * @brief Per-FILE recursive futex lock.
 * @Created: 2026/11/21 21:16:19 by Moutig
 * @Updated: 2026/11/21 21:41:08 by Moutig
 *
 * Implements the recursive lock carried inline in struct FILE
 * (fields lock and lockCount). The lock is owned by a thread id
 * (tid) and is re-entrant: flockfile() may be called several times
 * by the same thread, funlockfile() releases one level at a time,
 * and the lock is actually freed when lockCount drops to zero.
 *
 * While __haj_stdioThreaded is zero (no thread has ever been
 * created), both functions are no-ops: a single-threaded program
 * never pays the futex cost. pthread_create() sets the flag to one
 * before spawning the first thread, so every stream in the process
 * starts taking the lock from that point on.
 *
 * The lock word is an int: 0 means free, any other value is the
 * tid of the owner. Contention waits on the same word through
 * __haj_futexWait / __haj_futexWake (src/thread/internal/futex.c).
 */

#include "stdioImpl.h"

#include <errno.h>
#include <bits/thread/futex.h>
#include <bits/thread/thread.h>

/**
 * @brief Global mono/multi-thread switch for the stdio locks.
 *
 * Zero until pthread_create() spawns the first thread, then one
 * forever. Read by __haj_lockFile / __haj_unlockFile on every call,
 * written once by pthread_create() with a release store, read here
 * with an acquire load so the pairing also holds on aarch64.
 */
volatile int	__haj_stdioThreaded = 0;

void	__haj_lockFile(struct FILE *f)
{
	int	tid;
	int	owner;

	if (!__atomic_load_n(&__haj_stdioThreaded, __ATOMIC_ACQUIRE))
		return ;

	tid = __haj_gettid();
	owner = __atomic_load_n(&f->lock, __ATOMIC_RELAXED);

	for (;;)
	{
		if (owner == 0)
		{
			if (__atomic_compare_exchange_n(&f->lock, &owner, tid,
					0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
			{
				f->lockCount = 1;
				return ;
			}
			/* CAS failed: owner now holds the current value. */
			continue ;
		}
		if (owner == tid)
		{
			f->lockCount++;
			return ;
		}
		/*
		 * Contended: wait on the lock word. The kernel returns
		 * when the word changes or spuriously, so loop back and
		 * re-check.
		 */
		__haj_futexWaitOp((int *)&f->lock, owner, 0);
		owner = __atomic_load_n(&f->lock, __ATOMIC_RELAXED);
	}
}


int	__haj_tryLockFile(struct FILE *f)
{
	int	tid;
	int	owner;

	if (!__haj_stdioThreaded)
		return (0);
	tid = __haj_gettid();
	owner = __atomic_load_n(&f->lock, __ATOMIC_RELAXED);
	if (owner == 0)
	{
		if (__atomic_compare_exchange_n(&f->lock, &owner, tid,
				0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
		{
			f->lockCount = 1;
			return (0);
		}
		owner = __atomic_load_n(&f->lock, __ATOMIC_RELAXED);
	}
	if (owner == tid)
	{
		f->lockCount++;
		return (0);
	}
	errno = EBUSY;
	return (-1);
}

/**
 * @brief Release one level of the recursive lock of @p f.
 *
 * Decrements lockCount; when it reaches zero the lock word is
 * cleared and one waiter (if any) is woken.
 *
 * @param f Stream to unlock (must not be NULL and owned by the caller).
 */
void	__haj_unlockFile(struct FILE *f)
{
	int	tid;

	if (!__atomic_load_n(&__haj_stdioThreaded, __ATOMIC_ACQUIRE))
		return ;

	tid = __haj_gettid();
	if (__atomic_load_n(&f->lock, __ATOMIC_RELAXED) != tid)
	{
		/*
		 * Unlocking a stream the caller does not own is a bug in
		 * the stdio layer, not a user error: the public functions
		 * always pair lock/unlock. Stay silent in release builds;
		 * a debug build could abort here.
		 */
		return ;
	}
	f->lockCount--;
	if (f->lockCount == 0)
	{
		__atomic_store_n(&f->lock, 0, __ATOMIC_RELEASE);
		__haj_futexWakeOp((int *)&f->lock, 1, 0);
	}
}
