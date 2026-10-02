/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file join.c
 * @brief Implementation of pthread_join().
 * @Created: 2026/10/01 10:28:19 by Moutig
 * @Updated: 2026/10/02 07:08:25 by Moutig
 *
 * pthread_join blocks until the target thread has exited,
 * then reads its return value, removes it from the global
 * list, and frees its stack.
 *
 * Waiting is done on tcb->tid: the kernel clears it to 0 when
 * the thread exits (through CLONE_CHILD_CLEARTID) and does a
 * *non-private* FUTEX_WAKE on that address. So the joiner must
 * use a *non-private* FUTEX_WAIT to be woken.
 *
 * Race: between the load of tcb->tid and the FUTEX_WAIT, the
 * thread could have exited. FUTEX_WAIT handles that
 * atomically: if *uaddr != val, it returns EAGAIN immediately,
 * and we re-check the loop condition. No lost wakeup.
 */

#include <pthread.h>
#include <errno.h>
#include <bits/thread/thread.h>

int pthread_join(pthread_t thread, void **retval)
{
	struct __haj_tcb	*tcb;
	int					v;
	int					detached;

	if (thread == 0)
		return (EINVAL);

	tcb = (struct __haj_tcb *)thread;

	/*
	 * Refuse to join ourselves. Refuse to join a detached
	 * thread: the TCB may already be freed.
	 */
	if (tcb == __haj_tcbSelf())
		return (EDEADLK); /* Joining self is a deadlock. */

	detached = __haj_atomic_load(&tcb->detachState);
	if (detached == PTHREAD_CREATE_DETACHED)
		return (EINVAL);

	/*
	 * Wait for the kernel to clear tcb->tid (CLONE_CHILD_CLEARTID).
	 * Non-private FUTEX_WAIT, because the kernel uses a
	 * non-private wake on that address.
	 */
	while ((v = __haj_atomic_load(&tcb->tid)) != 0) {
		(void)__haj_futex((int *)&tcb->tid, FUTEX_WAIT, v,
						  NULL, NULL, 0);
	}

	if (retval != NULL)
		*retval = tcb->retval;

	/*
	 * Now the thread is dead and no one else is waiting on
	 * it. Remove from the live list and free the stack.
	 */
	__haj_threadListRemove(tcb);
	__haj_threadFreeStack(tcb->stackBase, tcb->stackSize, tcb->guardSize);

	return (0);
}
