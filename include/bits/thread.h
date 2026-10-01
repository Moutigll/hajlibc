/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file thread.h
 * @brief Threading internals.
 * @Created: 2026/09/30 05:18:03 by Moutig
 * @Updated: 2026/10/01 09:10:41 by Moutig
 *
 * These are the low-level pieces that <pthread.h> builds on top
 * of. They are not part of the public API.
 */

#ifndef _BITS_THREAD_H
# define _BITS_THREAD_H

# include <bits/types.h>
# include <bits/tcb.h>

/* ----- Sentinel values for pthread_t ----- */

/*
 * pthread_t is an opaque type (unsigned long in <bits/types.h>).
 * We store a pointer to the TCB in it, cast to unsigned long.
 * These sentinels are used internally:
 */

# define HAJ_PTHREAD_NULL	((pthread_t)0)
# define HAJ_PTHREAD_SELF	((pthread_t)-1)   /* pthread_self() of main thread */

/* ----- Atomic primitives ----- */
/**
 * We use GCC/Clang builtins for atomics. These are the only
 * operations we need for lock-free data structures.
 */

# if defined(__HAJ_COMPILER_GNULIKE)

#  define __haj_atomic_load(p)		__atomic_load_n((p), __ATOMIC_SEQ_CST)
#  define __haj_atomic_store(p, v)	__atomic_store_n((p), (v), __ATOMIC_SEQ_CST)

#  define __haj_atomic_cas(p, old, new) \
	__atomic_compare_exchange_n((p), (old), (new), 0, \
								__ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

#  define __haj_atomic_exchange(p, v)	__atomic_exchange_n((p), (v), __ATOMIC_SEQ_CST)
#  define __haj_atomic_add_fetch(p, v)	__atomic_add_fetch((p), (v), __ATOMIC_SEQ_CST)
#  define __haj_atomic_sub_fetch(p, v)	__atomic_sub_fetch((p), (v), __ATOMIC_SEQ_CST)

# else
#  error "hajlib requires GCC or Clang for atomics"
# endif

/* ----- CPU relaxation ----- */
/**
 * A hint to the CPU that we are in a spin loop. On x86 this is
 * the PAUSE instruction. On aarch64 it is YIELD.
 */

static __HAJ_INLINE void __haj_cpuRelax(void)
{
# if defined(HAJ_ARCH_X86_64) || defined(HAJ_ARCH_I386)
	__asm__ volatile ("pause" ::: "memory");
# elif defined(HAJ_ARCH_AARCH64) || defined(HAJ_ARCH_ARM)
	__asm__ volatile ("yield" ::: "memory");
# else
	/* Nothing. */
# endif
}

/**
 * @brief Clone a new thread.
 *
 * This is a wrapper around the system call that creates a new
 * thread. It sets up the stack and TCB for the new thread.
 */
long __haj_clone(int (*fn)(void *), void *child_stack, int flags, void *arg, int *ptid, void *tls, int *ctid);

/* ----- Kernel TID ----- */

/**
 * @brief Return the kernel thread id of the calling thread.
 *
 * On Linux, uses gettid(2). On FreeBSD, uses thr_self(2).
 * On Darwin, uses thread_selfid(2).
 */
int __haj_gettid(void);

/* ----- Global thread list ----- */

/**
 * @brief Add a thread to the global list.
 *
 * Takes the global lock, inserts the thread at the head, releases
 * the lock.
 */
void __haj_threadListAdd(struct __haj_tcb *tcb);

/**
 * @brief Remove a thread from the global list.
 */
void __haj_threadListRemove(struct __haj_tcb *tcb);

/* ----- Thread entry point ----- */

/**
 * @brief The trampoline that runs in a new thread.
 *
 * Called by the clone() wrapper in assembly. The argument is a
 * pointer to the TCB. The function:
 *   1. Reads start_routine and arg from the TCB.
 *   2. Calls start_routine(arg).
 *   3. Calls pthread_exit(retval).
 *   4. Never returns.
 */
void __haj_threadStart(struct __haj_tcb *tcb) __HAJ_NORETURN;

/* ----- Cancellation ----- */

/*
 * POSIX requires at least these two cancellation types and two
 * states. We define them here so that pthread.h can use them.
 */

# define HAJ_PTHREAD_CANCEL_ENABLE			0
# define HAJ_PTHREAD_CANCEL_DISABLE			1

# define HAJ_PTHREAD_CANCEL_DEFERRED		0
# define HAJ_PTHREAD_CANCEL_ASYNCHRONOUS	1

/* ----- Cleanup handlers ----- */

struct __haj_cleanup {
	struct __haj_cleanup	*next;
	void					(*routine)(void *);
	void					*arg;
};

/* ----- Scheduling policies ----- */

/*
 * POSIX defines three scheduling policies. Linux and FreeBSD
 * have their own numeric values, but POSIX requires these
 * symbolic names. The values match Linux's <sched.h>.
 */

# define HAJ_SCHED_OTHER	0
# define HAJ_SCHED_FIFO		1
# define HAJ_SCHED_RR		2

#endif /* _BITS_THREAD_H */
