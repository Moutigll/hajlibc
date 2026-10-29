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
 * @Updated: 2026/10/23 00:01:32 by Moutig
 *
 * The TCB holds all per-thread data the runtime needs: identity,
 * stack bounds, join state, TSD slots, cancellation state, and
 * cleanup handlers.
 *
 * Every thread owns one contiguous, 64-byte aligned "TLS area" that
 * holds its TLS variables AND its TCB (see tcb.c):
 *
 *   x86_64  (TLS variant II, TCB at TP):
 *
 *       area                          area + tlsSize
 *       | TLS variables (tlsSize) | TCB |
 *                                  ^-- TP (%fs), TP == tcb, %fs:0 == tcb
 *
 *   aarch64 (TLS variant I, TLS above TP):
 *
 *       area            area + span      TP + 16 (rounded to tls align)
 *       | TCB (span) |  TP -> | gap | TLS variables |
 *                       ^-- tpidr_el0 == tcb + span
 *
 * The thread register therefore does NOT always equal the TCB
 * pointer: always go through __haj_tcbSelf() / __haj_tcbToTp().
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
 * ABI-mandated offsets (x86_64):
 *   0x00  self          read by __haj_tcbSelf() as %fs:0
 *   0x08  dtv
 *   0x28  stackCanary   clang/gcc read the stack-protector canary
 *                       from %fs:0x28 (glibc's stack_guard slot).
 *
 * Everything else is free.
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

	/* ---- Canary (ABI-mandated, offset 0x28 on x86_64) ---- */
	__haj_uintptr			stackCanary;	/* stack protector guard value */

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

	/* ---- TSD slots ---- */
	void					*specific[PTHREAD_KEYS_MAX];
} __HAJ_ALIGNED(16);

_Static_assert(offsetof(struct __haj_tcb, self) == 0,
			   "self must be at offset 0 (read as %fs:0)");

# if defined(__x86_64__)
_Static_assert(offsetof(struct __haj_tcb, stackCanary) == 0x28,
			   "stackCanary must be at %fs:0x28 (stack protector ABI)");
# endif

/**
 * @brief Bytes reserved for the TCB in an aarch64 TLS area.
 *
 * sizeof(TCB) rounded up to 64 so that TP (= tcb + span) stays 64-byte
 * aligned. Only used on aarch64, where the TCB lives BELOW TP.
 */
# define __HAJ_TCB_SPAN		((sizeof(struct __haj_tcb) + 63u) & ~(size_t)63u)

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
 * @brief The main thread's TCB (NULL until __hajSetThreadRegister ran).
 *
 * The main thread's TCB lives in a static TLS area defined in tcb.c.
 * It is no longer a linker-script symbol.
 */
extern struct __haj_tcb	*__haj_main_tcb_ptr;

/*
 * Compatibility: code written against the old linker-defined
 * `struct __haj_tcb __haj_main_tcb` (e.g. `&__haj_main_tcb`) keeps
 * working, it now resolves to the pointer above.
 */
# define __haj_main_tcb		(*__haj_main_tcb_ptr)

/* ----- TLS area ----- */

/**
 * @brief Size in bytes of one thread's TLS area (TLS variables + TCB).
 *
 * Always a multiple of 64. The area must be allocated 64-byte aligned.
 */
size_t				__haj_tlsAreaSize(void);

/**
 * @brief Build a thread's TLS area in freshly allocated memory.
 *
 * Copies the .tdata image, zeroes .tbss, zeroes the TCB and sets
 * `self`, `selfId` and `state`. Every other TCB field is left zero for
 * the caller (pthread_create) to fill.
 *
 * @param area  64-byte aligned buffer of at least __haj_tlsAreaSize() bytes.
 * @return      The TCB of the new thread.
 */
struct __haj_tcb	*__haj_tlsAreaInit(void *area);

/* ----- Helpers ----- */

/**
 * @brief Value of the thread register (%fs base / tpidr_el0) for a TCB.
 *
 * This is the value to pass to arch_prctl(ARCH_SET_FS) or as the `tls`
 * argument of clone(CLONE_SETTLS).
 */
static __HAJ_INLINE void *__haj_tcbToTp(struct __haj_tcb *tcb)
{
# if defined(__aarch64__)
	return ((char *)tcb + __HAJ_TCB_SPAN);
# else
	return (tcb);
# endif
}

/**
 * @brief Return the TCB of the calling thread.
 *
 * On x86_64: reads %fs:0 (the TCB sits exactly at TP).
 * On aarch64: reads tpidr_el0 and steps back over the TCB.
 * Elsewhere: falls back to __haj_tcbSelfFallback().
 */
static __HAJ_INLINE struct __haj_tcb *__haj_tcbSelf(void)
{
	struct __haj_tcb *tcb;

# if defined(__x86_64__)
	__asm__ volatile ("movq %%fs:0, %0" : "=r" (tcb));
# elif defined(__aarch64__)
	char *tp;

	__asm__ volatile ("mrs %0, tpidr_el0" : "=r" (tp));
	tcb = (struct __haj_tcb *)(tp - __HAJ_TCB_SPAN);
# else
	tcb = __haj_tcbSelfFallback();
# endif

	return (tcb);
}

#endif /* _BITS_THREAD_TCB_H */
