/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file key.c
 * @brief Implementation of thread-specific data (TSD) / pthread_key_t.
 * @Created: 2026/10/02 11:13:17 by Moutig
 * @Updated: 2026/10/02 11:56:31 by Moutig
 *
 * Thread-specific data provides per-thread pointers indexed by
 * a pthread_key_t. Each thread's TCB has a specific[] array
 * of void * pointers, one per key.
 *
 * A key can have a destructor that is called when a thread
 * exits, for each non-NULL value still associated with that
 * key. See __haj_threadExit.
 *
 * Keys are allocated sequentially up to HAJ_PTHREAD_KEYS_MAX.
 * A key is freed by setting its destructor to a sentinel value
 * (NULL is a valid destructor meaning "no destructor").
 */

#include <pthread.h>
#include <errno.h>
#include <bits/thread/thread.h>
#include <bits/thread/tcb.h>

/*
 * Destructor table. Index = key. Each slot is:
 *   NULL                    -> the key is free
 *   HAJ_KEY_DTOR_IN_USE     -> the key is allocated, no destructor
 *   any other pointer       -> the key is allocated, with this destructor
 *
 * HAJ_KEY_DTOR_IN_USE is a sentinel address that is never a
 * real destructor. It is used because NULL is a valid value
 * for "no destructor".
 */
# define HAJ_KEY_DTOR_IN_USE	((void (*)(void *))1)

static void	(*g_keyDestructors[PTHREAD_KEYS_MAX])(void *);
static int	g_keyNext = 0;
static int	g_keyLock = 0;

/* ----- Lock helpers ----- */

static __HAJ_INLINE void keyLock(void)
{
	while (__haj_atomic_exchange(&g_keyLock, 1))
		__haj_cpuRelax();
}

static __HAJ_INLINE void keyUnlock(void)
{
	__haj_atomic_store(&g_keyLock, 0);
}



int pthread_key_create(pthread_key_t *key, void (*destructor)(void *))
{
	int	k;

	if (key == NULL)
		return (EINVAL);

	keyLock();

	if (g_keyNext >= PTHREAD_KEYS_MAX) {
		keyUnlock();
		return (EAGAIN);
	}

	k = g_keyNext++;
	__haj_atomic_store(&g_keyDestructors[k], destructor ? destructor : HAJ_KEY_DTOR_IN_USE);

	keyUnlock();

	*key = (pthread_key_t)k;
	return (0);
}

int pthread_key_delete(pthread_key_t key)
{
	if (key >= PTHREAD_KEYS_MAX)
		return (EINVAL);

	keyLock();
	__haj_atomic_store(&g_keyDestructors[key], NULL);
	keyUnlock();

	return (0);
}

int pthread_setspecific(pthread_key_t key, const void *value)
{
	struct __haj_tcb	*tcb;

	if (key >= PTHREAD_KEYS_MAX)
		return (EINVAL);

	tcb = __haj_tcbSelf();
	if (tcb == NULL)
		return (EINVAL);

	__haj_atomic_store(&tcb->specificUsed, 1);

	__haj_atomic_store((void **)&tcb->specific[key], (void *)value);
	return (0);
}

void *pthread_getspecific(pthread_key_t key)
{
	struct __haj_tcb	*tcb;

	if (key >= PTHREAD_KEYS_MAX)
		return (NULL);

	tcb = __haj_tcbSelf();
	if (tcb == NULL)
		return (NULL);

	return (__haj_atomic_load((void **)&tcb->specific[key]));
}

/* ----- Internal: called from __haj_threadExit ----- */

/*
 * Run all TSD destructors for the given thread. Called by
 * __haj_threadExit when a thread exits, before freeing its
 * TCB.
 *
 * POSIX requires iterating at least PTHREAD_DESTRUCTOR_ITERATIONS
 * times, calling each non-NULL specific[key] destructor and
 * clearing the slot. Values set again by a destructor are
 * picked up on the next iteration.
 */
void __haj_runTlsDestructors(struct __haj_tcb *tcb)
{
	int	iter;

	/*
	 * Fast path: if pthread_setspecific was never called for
	 * this thread, there is nothing to clean up. Skip the
	 * 128-iteration scan entirely.
	 *
	 * This is the common case: most threads never touch TSD.
	 */
	if (__haj_atomic_load(&tcb->specificUsed) == 0)
		return;

	for (iter = 0; iter < PTHREAD_DESTRUCTOR_ITERATIONS; iter++) {
		int	any = 0;

		for (int k = 0; k < PTHREAD_KEYS_MAX; k++) {
			void	(*dtor)(void *);
			void	*value;

			dtor = __haj_atomic_load(&g_keyDestructors[k]);
			if (dtor == NULL || dtor == HAJ_KEY_DTOR_IN_USE)
				continue;

			value = __haj_atomic_load((void **)&tcb->specific[k]);
			if (value == NULL)
				continue;

			__haj_atomic_store((void **)&tcb->specific[k], NULL);
			dtor(value);
			any = 1;
		}

		if (!any)
			break;
	}
}
