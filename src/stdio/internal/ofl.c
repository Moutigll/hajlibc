/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file ofl.c
 * @brief Global list of open FILE streams.
 * @Created: 2026/11/21 21:39:31 by Moutig
 * @Updated: 2026/11/21 22:05:04 by Moutig
 *
 * Every FILE opened through fopen/fdopen/freopen is registered here
 * so that fflush(NULL), exit and the pthread_atfork handler can walk
 * every stream without keeping their own registry. The list is
 * doubly linked through the prev/next fields of struct FILE, which
 * means fclose can remove an arbitrary stream in O(1).
 *
 * The list has its own futex-based mutex, distinct from the per-FILE
 * locks. __haj_oflLock / __haj_oflUnlock protect the list pointers;
 * __haj_oflForeach walks the list under that mutex and calls a
 * callback for each stream. When the trylock argument is non-zero,
 * the per-FILE lock is taken with a trylock so that a thread that
 * already owns a stream does not deadlock against itself
 * (fflush(NULL) and exit use this).
 *
 * The list is anchored on a static sentinel node, used as both head
 * and tail and initialised at compile time to point to itself: no
 * malloc and no lazy initialisation race.
 *
 * Lock order: the list lock is always taken before a per-FILE lock
 * (__haj_oflForeach does list -> FILE). Code that holds a FILE lock
 * must not take the list lock: fclose unlinks the stream before
 * locking it, or after unlocking it.
 */

#include "stdioImpl.h"
#include <errno.h>
#include <bits/thread/futex.h>
#include <bits/thread/thread.h>

/* ----- List storage ----- */

/**
 * @brief Sentinel node that anchors the open-file list.
 *
 * head.next is the first stream, head.prev is the last one. The
 * sentinel is never added to a stream list and never freed.
 */
static struct FILE	gOflHead = { .prev = &gOflHead, .next = &gOflHead };

/**
 * @brief Mutex protecting the list pointers.
 *
 * 0 = free, otherwise tid of the owner. Non-recursive on purpose:
 * callbacks must not try to lock the list again.
 */
static volatile int	gOflLock = 0;

/* ----- List mutex ----- */

void	__haj_oflLock(void)
{
	int	tid;
	int	owner;

	tid = __haj_gettid();
	owner = __atomic_load_n(&gOflLock, __ATOMIC_RELAXED);
	for (;;)
	{
		if (owner == 0)
		{
			if (__atomic_compare_exchange_n(&gOflLock, &owner, tid,
					0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
				return ;
			continue ;
		}
		if (owner == tid)
		{
			/*
			 * The list lock is non-recursive: re-entering it is
			 * a bug in the stdio layer. Keep the same behaviour
			 * as __haj_unlockFile: silently return to avoid a
			 * hard deadlock in release builds.
			 */
			return ;
		}
		__haj_futexWaitOp((int *)&gOflLock, owner, 0);
		owner = __atomic_load_n(&gOflLock, __ATOMIC_RELAXED);
	}
}

void	__haj_oflUnlock(void)
{
	int	tid;

	tid = __haj_gettid();
	if (__atomic_load_n(&gOflLock, __ATOMIC_RELAXED) != tid)
		return ;
	__atomic_store_n(&gOflLock, 0, __ATOMIC_RELEASE);
	__haj_futexWakeOp((int *)&gOflLock, 1, 0);
}

/* ----- List operations ----- */

void	__haj_oflAdd(struct FILE *f)
{
	if (f == NULL)
		return ;
	__haj_oflLock();
	f->prev = &gOflHead;
	f->next = gOflHead.next;
	gOflHead.next->prev = f;
	gOflHead.next = f;
	__haj_oflUnlock();
}

void	__haj_oflRemove(struct FILE *f)
{
	if (f == NULL)
		return ;
	__haj_oflLock();
	if (f->prev == NULL || f->next == NULL)
	{
		/* Not in the list (never added, or already removed). */
		__haj_oflUnlock();
		return ;
	}
	f->prev->next = f->next;
	f->next->prev = f->prev;
	f->prev = NULL;
	f->next = NULL;
	__haj_oflUnlock();
}

void	__haj_oflForeach(void (*cb)(struct FILE *, void *), void *arg, int trylock)
{
	struct FILE	*f;
	struct FILE	*next;

	if (cb == NULL)
		return ;
	__haj_oflLock();
	f = gOflHead.next;
	while (f != &gOflHead)
	{
		next = f->next;
		if (trylock)
		{
			if (__haj_tryLockFile(f) == 0)
			{
				cb(f, arg);
				__haj_unlockFile(f);
			}
		}
		else
		{
			__haj_lockFile(f);
			cb(f, arg);
			__haj_unlockFile(f);
		}
		f = next;
	}
	__haj_oflUnlock();
}
