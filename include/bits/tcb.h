/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file tcb.h
 * @brief Thread Control Block (TCB) structure and helpers.
 * @Created: 2026/09/30 05:17:29 by Moutig
 * @Updated: 2026/09/30 12:33:29 by Moutig
 *
 * Each thread has a Thread Control Block (TCB), a small structure
 * that stores per-thread data the runtime needs: the thread id,
 * the stack bounds, the join state, the thread-specific data
 * slots, the destructor list, and a few reserved fields for the
 * dynamic linker.
 *
 * The TCB is placed at a fixed offset inside the thread's TLS
 * block. On x86_64 the thread register %fs points to the TCB.
 * On aarch64 the thread register tpidr_el0 points to it. The
 * compiler-generated TLS access code reads the TCB from there.
 *
 * Do NOT include this header directly from user code. It is
 * internal to <pthread.h>.
 */

#ifndef _BITS_TCB_H
# define _BITS_TCB_H

# include <bits/compiler.h>
# include <bits/types.h>
# include <bits/os.h>

/* ----- cpu_set_t -----
 *
 * POSIX defines cpu_set_t for CPU affinity. Its size is
 * implementation-defined. We use 128 bytes, which covers up to
 * 1024 CPUs on any platform.
 */

# define HAJ_CPU_SETSIZE	1024
# define HAJ_NCPUBITS		(8 * sizeof(unsigned long))
# define HAJ_NCPUWORDS		(HAJ_CPU_SETSIZE / HAJ_NCPUBITS)

# define __HAJ_CPU_BIT(n)	(1UL << ((n) % HAJ_NCPUBITS))
# define __HAJ_CPU_WORD(n)	((n) / HAJ_NCPUBITS)

/**
 * @brief CPU set type.
 * @details This type is used to represent a set of CPUs.
 */
typedef struct {
	unsigned long __bits[HAJ_NCPUWORDS];
} cpu_set_t;
/* ----- Constants ----- */

/*
 * Number of thread-specific data slots. POSIX requires at least
 * _POSIX_THREAD_KEYS_MAX (128). glibc uses 1024, musl uses 128.
 * We use 128 to keep the TCB small.
 */
# ifndef HAJ_PTHREAD_KEYS_MAX
#  define HAJ_PTHREAD_KEYS_MAX 128
# endif

/*
 * Number of times the destructor list is walked before giving up.
 * POSIX requires at least PTHREAD_DESTRUCTOR_ITERATIONS (4).
 */
# ifndef HAJ_PTHREAD_DESTRUCTOR_ITERATIONS
#  define HAJ_PTHREAD_DESTRUCTOR_ITERATIONS 4
# endif

/*
 * Size of a thread stack, in bytes. POSIX requires at least
 * PTHREAD_STACK_MIN. We use 8 MiB, which matches the default
 * on Linux and Darwin.
 */
# ifndef HAJ_PTHREAD_STACK_SIZE_DEFAULT
#  define HAJ_PTHREAD_STACK_SIZE_DEFAULT  (8 * 1024 * 1024)
# endif

/*
 * Size of the guard page at the bottom of each thread stack.
 * One page is enough to catch most overflows.
 */
# ifndef HAJ_PTHREAD_GUARD_SIZE_DEFAULT
#  define HAJ_PTHREAD_GUARD_SIZE_DEFAULT (4 * 1024)
# endif

/* ----- TCB state ----- */

# define HAJ_THREAD_RUNNING		0
# define HAJ_THREAD_EXITED		1
# define HAJ_THREAD_DETACHED	2

/* ----- TCB structure ----- */

/**
 * @brief Thread Control Block (TCB) structure.
 *
 * The TCB is allocated at the top of the thread's stack. It is
 * aligned to 16 bytes. The thread register points to it.
 *
 * The TCB contains the thread's identity, stack bounds, join state,
 * thread-specific data, cancellation state, and cleanup handlers.
 */
struct __haj_tcb {
	/* ---- Mandatory fields (offset 0 and 8) ---- */
	void					*self;			/* self-pointer, must be at offset 0 */
	void					*dtv;			/* dynamic thread vector (reserved) */

	/* ---- Thread identity ---- */
	int						tid;			/* kernel TID (gettid) */
	pthread_t				selfId;			/* pthread_self() value */

	/* ---- Stack ---- */
	void					*stackBase;		/* bottom of the mapped region */
	size_t					stackSize;		/* total size of the region */
	void					*stackTop;		/* top of the stack (aligned) */
	size_t					guardSize;		/* size of the guard page */

	/* ---- Lifecycle ---- */
	int						state;			/* HAJ_THREAD_RUNNING, ... */
	int						joinFutex;		/* futex word for pthread_join */
	void					*retval;		/* value passed to pthread_exit */
	struct __haj_tcb		*next;			/* linked list of all threads */
	struct __haj_tcb		*prev;

	/* ---- Attributes ---- */
	int						detachState;	/* PTHREAD_CREATE_JOINABLE / DETACHED */
	int						schedPolicy;
	int						schedPriority;
	int						inheritsched;
	int						scope;
	cpu_set_t				*affinityMask;

	/* ---- Cancellation ---- */
	int						cancelState;	/* PTHREAD_CANCEL_ENABLE / DISABLE */
	int						cancelType;		/* PTHREAD_CANCEL_DEFERRED / ASYNCHRONOUS */
	int						cancelPending;	/* non-zero if cancel requested */

	/* ---- Thread-specific data ---- */
	void					*specific[HAJ_PTHREAD_KEYS_MAX];
	unsigned int			specificSeq;	/* sequence to handle destructor re-runs */

	/* ---- Stack canary (thread-local copy) ---- */
	__haj_uintptr			stackCanary;

	/* ---- Cleanup handlers (for pthread_cleanup_push/pop) ---- */
	struct __haj_cleanup	*cleanupStack;
};

/* ----- Global list of threads ----- */

/**
 * @brief Global list of all threads.
 *
 * The list is protected by __haj_threadListLock. The main thread
 * is always at the head of the list. Detached threads are removed
 * from the list when they exit.
 */
extern struct __haj_tcb	*__haj_threadList;
/**
 * @brief Lock for the global thread list.
 *
 * This is a simple spinlock. It is used to protect the global
 * thread list and the TCBs of detached threads. It is not used
 * for normal thread operations, which are lock-free.
 */
extern int				__haj_threadListLock;

/* ----- Helpers ----- */

/**
 * @brief Return the TCB of the calling thread.
 *
 * On x86_64, reads %fs:0. On aarch64, reads tpidr_el0. On other
 * platforms, uses a slower fallback.
 */
static __HAJ_INLINE struct __haj_tcb *__haj_tcbSelf(void)
{
	struct __haj_tcb *tcb;

# if defined(HAJ_ARCH_X86_64)
	__asm__ volatile ("movq %%fs:0, %0" : "=r" (tcb));
# elif defined(HAJ_ARCH_AARCH64)
	__asm__ volatile ("mrs %0, tpidr_el0" : "=r" (tcb));
# else
	tcb = __haj_tcbSelfFallback();
# endif

	return (tcb);
}

#endif /* _BITS_TCB_H */
