/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file detach.c
 * @brief Implementation of pthread_detach().
 * @Created: 2026/10/01 10:28:38 by Moutig
 * @Updated: 2026/10/02 07:09:53 by Moutig
 *
 * Marking a thread detached means its resources are freed as
 * soon as it exits, without requiring a pthread_join.
 *
 * Race to be careful about: the thread could exit between our
 * read of detachState and our write. If it exits as JOINABLE,
 * it stays in the list and its stack is not freed; nobody
 * will free it after we detach it, so we'd leak. To handle
 * that, we check tcb->tid after the write: if it is already
 * 0, the thread has exited and we can free its stack
 * ourselves.
 */

#include <pthread.h>
#include <errno.h>
#include <bits/thread/thread.h>

int pthread_detach(pthread_t thread)
{
	struct __haj_tcb	*tcb;
	int					v;

	if (thread == 0)
		return (EINVAL);

	tcb = (struct __haj_tcb *)thread;

	if (tcb == __haj_tcbSelf())
		return (EINVAL);

	/*
	 * If the thread is already dead, its stack has not been
	 * freed (it was JOINABLE). We do it now.
	 *
	 * If the thread is still running, it will observe the
	 * DETACHED state and free its own stack in
	 * __haj_threadExit.
	 */
	v = __haj_atomic_load(&tcb->tid);
	if (v == 0) {
		__haj_threadListRemove(tcb);
		__haj_threadFreeStack(tcb->stackBase,
							  tcb->stackSize,
							  tcb->guardSize);
		return (0);
	}

	__haj_atomic_store(&tcb->detachState, PTHREAD_CREATE_DETACHED);
	return (0);
}
