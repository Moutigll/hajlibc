/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file once.c
 * @brief Implementation of pthread_once.
 * @Created: 2026/10/02 11:12:16 by Moutig
 * @Updated: 2026/10/02 11:23:38 by Moutig
 *
 * pthread_once() guarantees that init_routine is called exactly
 * once, no matter how many threads call pthread_once with the
 * same once_control.
 *
 * Implementation:
 *
 *   once_control is an int with three states:
 *     HAJ_ONCE_UNINITIALIZED (0)  - init_routine not yet run
 *     HAJ_ONCE_RUNNING       (1)  - init_routine is running
 *     HAJ_ONCE_DONE          (2)  - init_routine has returned
 *
 *   The first thread to CAS 0 -> 1 runs init_routine, then sets
 *   the state to 2 and wakes any waiter.
 *
 *   Other threads that see 1 sleep on the futex word until the
 *   state becomes 2.
 *
 *   Threads that see 2 return immediately.
 *
 * This is the standard implementation used by glibc and musl.
 * We use PRIVATE futexes throughout.
 */

#include <pthread.h>
#include <bits/thread/thread.h>
#include <bits/thread/futex.h>
#include <errno.h>

# define HAJ_ONCE_UNINITIALIZED	0
# define HAJ_ONCE_RUNNING		1
# define HAJ_ONCE_DONE			2

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
	int	v;

	if (once_control == NULL || init_routine == NULL)
		return (EINVAL);

	v = HAJ_ONCE_UNINITIALIZED;
	if (__haj_atomic_cas(once_control, &v, HAJ_ONCE_RUNNING)) {
		/*
		 * We are the first to see 0. Run the
		 * initialization, then publish DONE and wake anyone
		 * waiting.
		 */
		init_routine();
		__haj_atomic_store(once_control, HAJ_ONCE_DONE);
		__haj_futexWake((int *)once_control, 0x7fffffff);
		return (0);
	}

	/*
	 * Someone else is running (or has run) the initialization.
	 * Wait until it is DONE.
	 */
	while ((v = __haj_atomic_load(once_control)) != HAJ_ONCE_DONE) {
		/*
		 * FUTEX_WAIT returns EAGAIN if the value changed
		 * between the load and the wait. That is fine, we
		 * just loop and re-check.
		 */
		(void)__haj_futex((int *)once_control, FUTEX_WAIT_PRIVATE, v,
						  NULL, NULL, 0);
	}

	return (0);
}
