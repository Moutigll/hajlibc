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
 * @Updated: 2026/10/02 09:02:52 by Moutig
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

# include <bits/compiler.h>
# include <bits/types.h>
# include <bits/os.h>
# include <bits/thread/pthreadtypes.h>

/* ----- cpu_set_t ----- */

# define HAJ_CPU_SETSIZE	1024
# define HAJ_NCPUBITS		(8 * sizeof(unsigned long))
# define HAJ_NCPUWORDS		(HAJ_CPU_SETSIZE / HAJ_NCPUBITS)

# define __HAJ_CPU_BIT(n)	(1UL << ((n) % HAJ_NCPUBITS))
# define __HAJ_CPU_WORD(n)	((n) / HAJ_NCPUBITS)

/**
 * @brief CPU set type.
 *
 * Used by pthread_setaffinity_np / pthread_getaffinity_np and
 * by sched_setaffinity / sched_getaffinity.
 */
typedef struct {
	unsigned long __bits[HAJ_NCPUWORDS];
} cpu_set_t;

/* ----- Constants ----- */

# ifndef HAJ_PTHREAD_KEYS_MAX
#  define HAJ_PTHREAD_KEYS_MAX 128
# endif

# ifndef HAJ_PTHREAD_DESTRUCTOR_ITERATIONS
#  define HAJ_PTHREAD_DESTRUCTOR_ITERATIONS 4
# endif

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

struct __haj_cleanup;

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
	struct __haj_cleanup	*cleanupStack;

	/* ---- Attributes (4-byte) ---- */
	int						detachState;	/* JOINABLE / DETACHED */
	int						schedPolicy;	/* HAJ_SCHED_* */
	int						schedPriority;
	int						inheritsched;	/* INHERIT / EXPLICIT */
	int						scope;			/* SYSTEM / PROCESS */
	int						cancelState;	/* ENABLE / DISABLE */
	int						cancelType;		/* DEFERRED / ASYNCHRONOUS */
	int						cancelPending;	/* non-zero if requested */
	unsigned int			specificSeq;	/* TSD destructor iteration */

	/* ---- Affinity ---- */
	cpu_set_t				*affinityMask;

	/* ---- Canary ---- */
	__haj_uintptr			stackCanary;

	/* ---- TSD slots ---- */
	void					*specific[HAJ_PTHREAD_KEYS_MAX];
};

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
