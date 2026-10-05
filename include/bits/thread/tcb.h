/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file tcb.h
 * @brief Thread Control Block (TCB).
 * @Created: 2026/09/30 05:17:29 by Moutig
 * @Updated: 2026/10/05 11:39:37 by Moutig
 *
 * The TCB holds all per-thread data the runtime needs: identity,
 * stack bounds, join state, TSD slots, cancellation state, and
 * cleanup handlers.
 *
 * It is placed at the top of the thread's stack, 16-byte
 * aligned. The thread register (%fs on x86_64, tpidr_el0 on
 * aarch64) points to it.
 *
 * Fields are ordered for a dense layout: pointers and size_t
 * first (8-byte aligned), then ints (4-byte), then arrays.
 *
 * PRIVATE header. Do not include from user code.
 */

#ifndef _BITS_THREAD_TCB_H
# define _BITS_THREAD_TCB_H

# include <bits/cpuSet.h>
# include <bits/thread/pthreadtypes.h>
# include <stddef.h>

/* ----- Robust mutex list ----- */

# if HAJ_PTHREAD_PROCESS_SHARED

/**
 * @brief Node in a robust mutex list.
 *
 * Each thread has a list of all the robust mutexes it owns. The
 * kernel uses this list to mark them as inconsistent if the
 * thread dies while holding them.
 */
struct _hajRobustNode { struct _hajRobustNode *next; };

/**
 * @brief Head of a robust mutex list.
 *
 * Each thread has a single head, which points to the circular
 * list of all the robust mutexes it owns. The kernel uses this
 * list to mark them as inconsistent if the thread dies while
 * holding them.
 */
struct _hajRobustHead {
	struct _hajRobustNode	list;			/* Circular list of owned mutexes */
	long					futexOffset;	/* Offset of the futex word in the mutex struct */
	struct _hajRobustNode	*pending;		/* List of mutexes pending to be added to the list */
};

_Static_assert(sizeof(struct _hajRobustHead) == 24,
			   "_hajRobustHead must match kernel ABI (24 bytes)");

_Static_assert(offsetof(struct _hajRobustHead, futexOffset) == 8,
			   "_hajRobustHead must match kernel ABI (futexOffset at offset 8)");
_Static_assert(offsetof(struct _hajRobustHead, pending) == 16,
			   "_hajRobustHead must match kernel ABI (pending at offset 16)");

/**
 * @brief Initialize a robust mutex list.
 * @param h The robust mutex list to initialize.
 */
void __haj_robustInit(struct _hajRobustHead *h);

# endif /* HAJ_PTHREAD_PROCESS_SHARED */

/* ----- Constants ----- */

# ifndef HAJ_PTHREAD_STACK_SIZE_DEFAULT
#  define HAJ_PTHREAD_STACK_SIZE_DEFAULT	(8 * 1024 * 1024)
# endif

# ifndef HAJ_PTHREAD_GUARD_SIZE_DEFAULT
#  define HAJ_PTHREAD_GUARD_SIZE_DEFAULT	(4096)
# endif

/* ----- TCB state ----- */

# define HAJ_THREAD_RUNNING		0
# define HAJ_THREAD_EXITED		1
# define HAJ_THREAD_DETACHED	2

/* ----- Forward declarations ----- */

struct __haj_thCleanup;

/* ----- TCB ----- */

/**
 * @brief Thread Control Block.
 *
 * Offset 0 and 8 are reserved: the first two pointers are
 * mandated by the ABI (self, dtv). Everything else is free.
 */
struct __haj_tcb {
	/* ---- ABI-mandated (offset 0, 8) ---- */
	void					*self;			/* must be at offset 0 */
	void					*dtv;			/* dynamic thread vector */

	/* ---- Identity (mixed) ---- */
	pthread_t				selfId;			/* pthread_self() value */
	int						tid;			/* kernel TID */
	int						state;			/* HAJ_THREAD_* */
	int						joinFutex;		/* futex for pthread_join */
	int						_pad0;			/* keep 8-byte alignment */

	/* ---- Stack ---- */
	void					*stackBase;		/* bottom of the mapping */
	void					*stackTop;		/* top of the stack */
	size_t					stackSize;		/* size of the mapping */
	size_t					guardSize;		/* guard page size */

	/* ---- Lifecycle ---- */
	void					*retval;		/* pthread_exit argument */
	struct __haj_tcb		*next;			/* linked list */
	struct __haj_tcb		*prev;

	/* ---- Entry point ---- */
	void					*(*startRoutine)(void *);
	void					*startArg;

	/* ---- Cleanup ---- */
	struct __haj_thCleanup	*cleanupStack;
# if HAJ_PTHREAD_PROCESS_SHARED
	struct _hajRobustHead	robustList;		/* list of robust mutexes */
# endif

	/* ---- Attributes (4-byte) ---- */
	int						detachState;	/* JOINABLE / DETACHED */
	int						schedPolicy;	/* HAJ_SCHED_* */
	int						schedPriority;
	int						inheritsched;	/* INHERIT / EXPLICIT */
	int						scope;			/* SYSTEM / PROCESS */
	int						cancelState;	/* ENABLE / DISABLE */
	int						cancelType;		/* DEFERRED / ASYNCHRONOUS */
	int						cancelPending;	/* non-zero if requested */
	unsigned int			specificUsed;	/* non-zero once pthread_setspecific has run */

	/* ---- Affinity ---- */
	cpu_set_t				*affinityMask;

	/* ---- Canary ---- */
	__haj_uintptr			stackCanary;

	/* ---- TSD slots ---- */
	void					*specific[PTHREAD_KEYS_MAX];
} __HAJ_ALIGNED(16);

/* ----- Globals ----- */

/**
 * @brief The list of all threads.
 */
extern struct __haj_tcb	*__haj_threadList;

/**
 * @brief Lock for the thread list.
 */
extern int				__haj_threadListLock;

/**
 * @brief The main thread's TCB.
 *
 * It is declared here so that the linker sees a single definition;
 * the symbol __haj_main_tcb is exported through <bits/thread/tcb.h>
 * so both the startup and other internals (e.g. pthread_self) can find it.
 */
extern struct __haj_tcb	__haj_main_tcb;

/* ----- Helpers ----- */

/**
 * @brief Return the TCB of the calling thread.
 *
 * On x86_64: reads %fs:0.
 * On aarch64: reads tpidr_el0.
 * Elsewhere: falls back to __haj_tcbSelfFallback().
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

#endif /* _BITS_THREAD_TCB_H */
