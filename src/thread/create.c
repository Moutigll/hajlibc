/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file create.c
 * @brief Implementation of pthread_create().
 * @Created: 2026/10/01 10:27:30 by Moutig
 * @Updated: 2026/10/05 12:35:56 by Moutig
 *
 * pthread_create allocates a stack from the reuse pool, places
 * a TCB at the top of it, initializes the TCB with the start
 * routine and argument, and calls clone() (or clone3()) to
 * start the new thread.
 *
 * The kernel writes the child TID into tcb->tid (through
 * CLONE_PARENT_SETTID) and clears it to 0 when the child exits
 * (through CLONE_CHILD_CLEARTID), futex-waking any joiner.
 * This is what makes pthread_join possible without a separate
 * futex.
 *
 * On the first successful call, an atexit() handler is
 * registered to flush the stack reuse pool when the process
 * exits normally. The registration is guarded by an atomic
 * flag so that concurrent first calls only register once.
 *
 * Errors are returned as positive errno values, per POSIX.
 */

#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <bits/thread/thread.h>
#include <bits/thread/pthread.h>

/* ----- One-shot atexit registration ----- */

/*
 * Registered exactly once, on the first successful
 * pthread_create call. The double-checked pattern below avoids
 * a lock on the hot path: the common case is a single atomic
 * load that reads 1.
 */
static int hajFlushRegistered = 0;

static void hajRegisterFlush(void)
{
	int expected;

	if (__haj_atomic_load(&hajFlushRegistered) != 0)
		return;

	expected = 0;
	if (__haj_atomic_cas(&hajFlushRegistered, &expected, 1))
		atexit(__haj_stackCacheFlush);
}

/* ----- pthread_create ----- */

/**
 * @brief Initialize a thread attribute structure with default values.
 * @param dst The attribute structure to initialize.
 */
static void hajAttrDefaults(struct _hajThreadAttr *dst)
{
	dst->stacksize		= HAJ_PTHREAD_STACK_SIZE_DEFAULT;
	dst->guardsize		= HAJ_PTHREAD_GUARD_SIZE_DEFAULT;
	dst->stackaddr		= NULL;
	dst->detachstate	= PTHREAD_CREATE_JOINABLE;
	dst->schedpolicy	= SCHED_OTHER;
	dst->schedpriority	= 0;
	dst->inheritsched	= PTHREAD_INHERIT_SCHED;
	dst->scope			= PTHREAD_SCOPE_SYSTEM;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
	struct _hajThreadAttr	defaults;
	const struct _hajThreadAttr	*a;
	struct __haj_tcb	*tcb;
	int			detachstate;
	size_t		stacksize;
	size_t		guardsize;
	void		*base;
	void		*top;
	void		*childStack;
	size_t		effSize;
	size_t		effGuard;
	long		r;

	if (thread == NULL || start_routine == NULL)
		return (EINVAL);

	/*
	 * Resolve the attribute source. A NULL attr means default
	 * values. We always point `a` at a valid struct so the
	 * rest of the function can dereference it unconditionally.
	 */
	if (attr != NULL) {
		a = HAJ_ATTR_CONST(attr);
	} else {
		hajAttrDefaults(&defaults);
		a = &defaults;
	}

	/* Normalize defaults. */
	detachstate	= a->detachstate;
	stacksize	= a->stacksize;
	guardsize	= a->guardsize;

	if (stacksize == 0)
		stacksize = HAJ_PTHREAD_STACK_SIZE_DEFAULT;
	if (guardsize == 0)
		guardsize = HAJ_PTHREAD_GUARD_SIZE_DEFAULT;

	/*
	 * Allocate the stack. The function returns the *effective*
	 * (page-aligned) sizes; those are what we store in the
	 * TCB, because that is what __haj_threadFreeStack expects
	 * when the thread dies.
	 */
	base = __haj_threadAllocStack(stacksize, guardsize, &effSize, &effGuard);
	if (base == NULL)
		return (EAGAIN);

	/*
	 * The top of the stack is at base + effSize + effGuard.
	 * __haj_tcbCreate places the TCB just below that and
	 * returns the address to pass to clone() as the child's
	 * stack pointer.
	 */
	top = (char *)base + effSize + effGuard;

	tcb = __haj_tcbCreate(base, effSize, effGuard, top, &childStack);
	if (tcb == NULL) {
		__haj_threadFreeStack(base, effSize, effGuard);
		return (EAGAIN);
	}

	tcb->startRoutine	= start_routine;
	tcb->startArg		= arg;
	tcb->detachState	= detachstate;
	tcb->schedPolicy	= a->schedpolicy;
	tcb->schedPriority	= a->schedpriority;
	tcb->inheritsched	= a->inheritsched;
	tcb->scope			= a->scope;

	/*
	 * Publish the TCB in the global thread list. If the child
	 * died in the meantime and was detached, it already
	 * called __haj_threadListRemove on a TCB that was not
	 * listed; that function is idempotent and does nothing.
	 */
	__haj_threadListAdd(tcb);

	/*
	 * Create the thread. __haj_clone dispatches between
	 * clone3(2) and clone(2) and caches the choice.
	 *
	 * The kernel writes the TID into tcb->tid (through
	 * CLONE_PARENT_SETTID) and clears it on exit (through
	 * CLONE_CHILD_CLEARTID), futex-waking any joiner.
	 *
	 * tls = tcb: on aarch64, CLONE_SETTLS makes the kernel
	 * write this pointer into tpidr_el0. On x86_64, it goes
	 * into fs_base. __haj_tcbSelf() reads it back.
	 */
	r = __haj_clone(__haj_threadTrampoline, childStack,
				 HAJ_CLONE_THREAD_FLAGS, tcb,
				 (int *)&tcb->tid,	/* parent_tid */
				 tcb,					/* tls */
				 (int *)&tcb->tid);	/* child_tid */
	if (r < 0) {
		/*
		 * The thread was never created: undo the list
		 * insertion and return the stack to the pool. We
		 * must not touch the TCB after freeing its stack.
		 */
		__haj_threadListRemove(tcb);
		__haj_threadFreeStack(base, effSize, effGuard);
		return ((int)-r);
	}

	/*
	 * Register the cache flush handler on first success. We
	 * do this after the thread has actually been created, so
	 * a program that only ever fails at pthread_create never
	 * registers an atexit handler for nothing.
	 */
	hajRegisterFlush();

	*thread = (pthread_t)tcb;
	return (0);
}
