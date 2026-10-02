/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file start.c
 * @brief Thread entry trampoline and exit.
 * @Created: 2026/10/01 10:24:39 by Moutig
 * @Updated: 2026/10/02 11:19:28 by Moutig
 *
 * __haj_threadStart is the function the clone wrapper invokes
 * in the child. It reads startRoutine/startArg from the TCB,
 * calls the user function, then calls __haj_threadExit.
 */

#include <bits/thread/thread.h>
#include <bits/syscall.h>
#include <pthread.h>

void __haj_threadExit(void *retval)
{
	struct __haj_tcb *tcb = __haj_tcbSelf();
	void	*stackBase	= tcb->stackBase;
	size_t	stackSize	= tcb->stackSize;
	size_t	stackGuard	= tcb->guardSize;
	int		detached	= (tcb->detachState == PTHREAD_CREATE_DETACHED);

	/*
	 * Run all TSD destructors for this thread. This is done
	 * before removing the TCB from the live list, so that
	 * destructors can still call pthread_getspecific().
	 */
	__haj_runTlsDestructors(tcb);
	tcb->retval	= retval;
	tcb->state	= HAJ_THREAD_EXITED;

	/*
	 * Remove from the live list FIRST. After this point the TCB
	 * is no longer reachable by other threads scanning the list.
	 * The joiner (if any) still holds a direct pointer to it and
	 * reads `retval`/`state` from it, so the memory must stay
	 * valid until the joiner is done.
	 */
	__haj_threadListRemove(tcb);

	/*
	 * Wake any thread waiting in pthread_join. Do this AFTER
	 * removing from the list, so the joiner never observes a
	 * TCB that is still linked into the list.
	 */
	__haj_futexWake(&tcb->joinFutex, 0x7fffffff);

	if (detached) {
		/*
		 * No one will join us: nobody else will read the TCB
		 * after this point, so we can hand the stack back to
		 * the cache. __haj_threadFreeStack never munmaps the
		 * stack it is given (it evicts an older entry instead),
		 * so this is safe even though we are running on it.
		 */
		__haj_threadFreeStack(stackBase, stackSize, stackGuard);
	}
	/*
	 * Joinable: the joiner is responsible for calling
	 * __haj_threadFreeStack once it has read `retval`. We do
	 * NOT touch the stack here.
	 */

	__haj_syscall1(SYS_exit, (long)retval);
	__builtin_unreachable();
}

void __haj_threadStart(struct __haj_tcb *tcb)
{
	void *retval;

	retval = tcb->startRoutine(tcb->startArg);
	__haj_threadExit(retval);
}

int __haj_threadTrampoline(void *arg)
{
	struct __haj_tcb *tcb = (struct __haj_tcb *)arg;

	__haj_threadStart(tcb);

	/* Not reached: __haj_threadStart does not return. */
	__builtin_unreachable();
}
