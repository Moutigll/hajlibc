/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file pthread_attr.h
 * @brief Internal layout of pthread_attr_t.
 * @Created: 2026/10/01 11:00:00 by Moutig
 * @Updated: 2026/10/03 13:25:37 by Moutig
 *
 * PRIVATE header. Included only by pthread_attr.c and
 * pthread_create.c.
 *
 * The public pthread_attr_t is a union of a 48-byte char buffer
 * and a long (for alignment). This struct is what we actually
 * store inside it. All accesses go through HAJ_ATTR().
 *
 * The _Static_assert below fails the build if the internal
 * struct no longer fits in the public buffer. That is the
 * signal to bump __SIZEOF_PTHREAD_ATTR_T (an ABI change).
 */

#ifndef _BITS_THREAD_PTHREAD_ATTR_H
# define _BITS_THREAD_PTHREAD_ATTR_H

# include <bits/thread/pthreadtypes.h>
# include <bits/thread/tcb.h>

/* ----- pthread attributes ----- */

/**
 * @brief Internal representation of pthread_attr_t.
 *
 * This struct represents the internal layout of a pthread
 * attribute object. It contains fields for stack size, guard
 * size, stack address, detach state, scheduling policy,
 * scheduling priority, inherit scheduler setting, and scope.
 */
struct _hajThreadAttr {
	/* ---- 8-byte ---- */
	size_t		stacksize;
	size_t		guardsize;
	void		*stackaddr;

	/* ---- 4-byte ---- */
	int			detachstate;
	int			schedpolicy;
	int			schedpriority;
	int			inheritsched;
	int			scope;
} __HAJ_ALIGNED(__ALIGNOF_PTHREAD_T);

/* The internal struct must fit in the public opaque type. */
_Static_assert(sizeof(struct _hajThreadAttr) == sizeof(pthread_attr_t),
			   "_hajThreadAttr does not fit in pthread_attr_t "
			   "(bump __SIZEOF_PTHREAD_ATTR_T)");
_Static_assert(_Alignof(struct _hajThreadAttr) == _Alignof(pthread_attr_t),
			   "_hajThreadAttr alignment exceeds pthread_attr_t "
			   "(bump __ALIGNOF_PTHREAD_T)");

/**
 * @brief Cast a public pthread_attr_t to its internal struct.
 *
 * The public type is a union containing a char buffer; casting
 * to a struct pointer is legal and the only field access path.
 */
# define HAJ_ATTR(p) \
	((struct _hajThreadAttr *)(void *)(p))
# define HAJ_ATTR_CONST(p) \
	((const struct _hajThreadAttr *)(const void *)(p))

/* ----- Mutex internal ----- */

/**
 * @brief Internal representation of pthread_mutex_t.
 *
 * This struct represents the internal layout of a pthread
 * mutex object. It contains fields for the lock state, mutex
 * type, owner thread ID, recursive lock count, and robustness.
 */
struct _hajThreadMutex {
	int	lock;	/* futex word: 0 = free, 1 = locked, 2 = contended */
	int	type;	/* HAJ_MUTEX_NORMAL / RECURSIVE / ERRORCHECK */
	int	owner;	/* TID of the owning thread */
	int	count;	/* recursive lock count */
	int	robust;	/* non-zero if robust */
#if HAJ_PTHREAD_PROCESS_SHARED
	int	pshared;	/* non-zero if process-shared */
	struct _hajRobustNode	robustNext;	/* next node in the robust mutex list */
#endif
} __HAJ_ALIGNED(__ALIGNOF_PTHREAD_T);

_Static_assert(sizeof(struct _hajThreadMutex) == sizeof(pthread_mutex_t),
			   "_hajThreadMutex does not fit in pthread_mutex_t "
			   "(bump __SIZEOF_PTHREAD_MUTEX_T)");
_Static_assert(_Alignof(struct _hajThreadMutex) == _Alignof(pthread_mutex_t),
			   "_hajThreadMutex alignment exceeds pthread_mutex_t "
			   "(bump __ALIGNOF_PTHREAD_MUTEX_T)");

# define HAJ_MUTEX(p) \
	((struct _hajThreadMutex *)(void *)(p))
# define HAJ_MUTEX_CONST(p) \
	((const struct _hajThreadMutex *)(const void *)(p))

# if HAJ_PTHREAD_PROCESS_SHARED
#  define HAJ_MUTEX_IS_SHARED(m)	((m)->pshared == PTHREAD_PROCESS_SHARED)
# else
#  define HAJ_MUTEX_IS_SHARED(m)	(0)
# endif

/* ----- Mutex attributes ----- */

/**
 * @brief Internal representation of pthread_mutexattr_t.
 *
 * This struct represents the internal layout of a pthread
 * mutex attributes object. It contains fields for mutex type,
 * process sharing, robustness, protocol, and priority ceiling.
 */
struct _hajThreadMutexAttr {
	int	type;
	int	pshared;
	int	robust;
	int	protocol;
	int	prioceiling;
} __HAJ_ALIGNED(__ALIGNOF_PTHREAD_T);

_Static_assert(sizeof(struct _hajThreadMutexAttr) == sizeof(pthread_mutexattr_t),
			   "_hajThreadMutexAttr does not fit in pthread_mutexattr_t "
			   "(bump __SIZEOF_PTHREAD_MUTEXATTR_T)");
_Static_assert(_Alignof(struct _hajThreadMutexAttr) == _Alignof(pthread_mutexattr_t),
			   "_hajThreadMutexAttr alignment exceeds pthread_mutexattr_t "
			   "(bump __ALIGNOF_PTHREAD_T)");

# define HAJ_MUTEXATTR(p) \
	((struct _hajThreadMutexAttr *)(void *)(p))
# define HAJ_MUTEXATTR_CONST(p) \
	((const struct _hajThreadMutexAttr *)(const void *)(p))

/* ----- Condition variables ----- */

/**
 * @brief Internal representation of pthread_cond_t.
 *
 * This struct represents the internal layout of a pthread condition
 * variable object. It contains fields for a generation counter and
 * the clock type used for timed waits.
 */
struct _hajThreadCond {
	int	seq;	/* futex word: generation counter */
	int	clock;	/* CLOCK_REALTIME or CLOCK_MONOTONIC */
#if HAJ_PTHREAD_PROCESS_SHARED
	int	pshared;	/* non-zero if process-shared */
#endif
} __HAJ_ALIGNED(__ALIGNOF_PTHREAD_T);

_Static_assert(sizeof(struct _hajThreadCond) == sizeof(pthread_cond_t),
			   "_hajThreadCond does not fit in pthread_cond_t "
			   "(bump __SIZEOF_PTHREAD_COND_T)");
_Static_assert(_Alignof(struct _hajThreadCond) == _Alignof(pthread_cond_t),
			   "_hajThreadCond alignment exceeds pthread_cond_t "
			   "(bump __ALIGNOF_PTHREAD_T)");

# define HAJ_COND(p) \
	((struct _hajThreadCond *)(void *)(p))
# define HAJ_COND_CONST(p) \
	((const struct _hajThreadCond *)(const void *)(p))

# if HAJ_PTHREAD_PROCESS_SHARED
#  define HAJ_COND_IS_SHARED(c)	((c)->pshared == PTHREAD_PROCESS_SHARED)
# else
#  define HAJ_COND_IS_SHARED(c)	(0)
# endif

/* ----- Condition variable attributes ----- */

/*
 * The attr holds:
 *   - clock: which clock to use for timedwait (REALTIME or MONOTONIC)
 *   - pshared: process sharing (accepted, not used)
 *
 * Two ints = 8 bytes, which is exactly the public size.
 */
struct _hajThreadCondAttr {
	int	clock;
	int	pshared;
} __HAJ_ALIGNED(__ALIGNOF_PTHREAD_T);

_Static_assert(sizeof(struct _hajThreadCondAttr) == sizeof(pthread_condattr_t),
			   "_hajThreadCondAttr does not fit in pthread_condattr_t "
			   "(bump __SIZEOF_PTHREAD_CONDATTR_T)");
_Static_assert(_Alignof(struct _hajThreadCondAttr) == _Alignof(pthread_condattr_t),
			   "_hajThreadCondAttr alignment exceeds pthread_condattr_t "
			   "(bump __ALIGNOF_PTHREAD_T)");

# define HAJ_CONDATTR(p) \
	((struct _hajThreadCondAttr *)(void *)(p))
# define HAJ_CONDATTR_CONST(p) \
	((const struct _hajThreadCondAttr *)(const void *)(p))

/* ----- Read-write locks ----- */

/*
 * The state word encodes:
 *   0                        free
 *   N > 0                    N readers active
 *   HAJ_RWLOCK_WRITER        a writer holds the lock
 *   HAJ_RWLOCK_WRWAIT        a writer is blocked, gate readers
 *   bits 0-29                reader count (when WRITER and WRWAIT are clear)
 *
 * A writer that cannot acquire sets WRWAIT and blocks. Readers
 * block while WRWAIT is set, so writers do not starve.
 */
# define HAJ_RWLOCK_WRITER	0x40000000u
# define HAJ_RWLOCK_WRWAIT	0x80000000u
# define HAJ_RWLOCK_RD_MASK	0x3fffffffu

/*
 * The rwlock struct holds:
 *   - state: the futex word (see above)
 *   - pshared: PTHREAD_PROCESS_SHARED or PRIVATE
 *   - _pad: padding to make the struct ABI-stable size
 */
struct _hajThreadRwlock {
	unsigned int	state;		/* futex word */
#if HAJ_PTHREAD_PROCESS_SHARED
	int				pshared;	/* PTHREAD_PROCESS_SHARED or PRIVATE */
#endif
} __HAJ_ALIGNED(__ALIGNOF_PTHREAD_T);

_Static_assert(sizeof(struct _hajThreadRwlock) == sizeof(pthread_rwlock_t),
			   "_hajThreadRwlock does not fit in pthread_rwlock_t");
_Static_assert(_Alignof(struct _hajThreadRwlock) == _Alignof(pthread_rwlock_t),
			   "_hajThreadRwlock alignment exceeds pthread_rwlock_t");

# define HAJ_RWLOCK(p)				((struct _hajThreadRwlock *)(void *)(p))
# define HAJ_RWLOCK_CONST(p)		((const struct _hajThreadRwlock *)(const void *)(p))

# if HAJ_PTHREAD_PROCESS_SHARED
#  define HAJ_RWLOCK_IS_SHARED(r)	((r)->pshared == PTHREAD_PROCESS_SHARED)
# else
#  define HAJ_RWLOCK_IS_SHARED(r)	(0)
# endif

/* ----- Read-write lock attributes ----- */

/*
 * The rwlock attr holds:
 *   - pshared: PTHREAD_PROCESS_SHARED or PRIVATE
 *   - _pad: padding to make the struct ABI-stable size
 */
struct _hajThreadRwlockAttr {
	int	pshared;
} __HAJ_ALIGNED(__ALIGNOF_PTHREAD_T);

_Static_assert(sizeof(struct _hajThreadRwlockAttr) == sizeof(pthread_rwlockattr_t),
			   "_hajThreadRwlockAttr does not fit in pthread_rwlockattr_t");
_Static_assert(_Alignof(struct _hajThreadRwlockAttr) == _Alignof(pthread_rwlockattr_t),
			   "_hajThreadRwlockAttr alignment exceeds pthread_rwlockattr_t");

# define HAJ_RWLOCKATTR(p)			((struct _hajThreadRwlockAttr *)(void *)(p))
# define HAJ_RWLOCKATTR_CONST(p)	((const struct _hajThreadRwlockAttr *)(const void *)(p))

/* ----- Barriers ----- */

/*
 * total   : number of threads required to cross the barrier
 * count   : number of threads currently arrived (atomic)
 * seq     : generation counter, used as the futex word
 *
 * The last thread to arrive resets count to 0, bumps seq and
 * wakes everyone. All waiters block on seq, so the futex word
 * is seq, not count.
 */
struct _hajThreadBarrier {
	unsigned int	total;
	int				count;
	int				seq;
#if HAJ_PTHREAD_PROCESS_SHARED
	int				pshared;
#endif
} __HAJ_ALIGNED(__ALIGNOF_PTHREAD_T);

_Static_assert(sizeof(struct _hajThreadBarrier) == sizeof(pthread_barrier_t),
			   "_hajThreadBarrier does not fit in pthread_barrier_t");
_Static_assert(_Alignof(struct _hajThreadBarrier) == _Alignof(pthread_barrier_t),
			   "_hajThreadBarrier alignment exceeds pthread_barrier_t");

# define HAJ_BARRIER(p)				((struct _hajThreadBarrier *)(void *)(p))
# define HAJ_BARRIER_CONST(p)		((const struct _hajThreadBarrier *)(const void *)(p))

# if HAJ_PTHREAD_PROCESS_SHARED
#  define HAJ_BARRIER_IS_SHARED(b)	((b)->pshared == PTHREAD_PROCESS_SHARED)
# else
#  define HAJ_BARRIER_IS_SHARED(b)	(0)
# endif

/* ----- Barrier attributes ----- */

/*
 * The barrier attr holds:
 *   - pshared: PTHREAD_PROCESS_SHARED or PRIVATE
 *   - _pad: padding to make the struct ABI-stable size
 */
struct _hajThreadBarrierAttr {
	int	pshared;
} __HAJ_ALIGNED(__ALIGNOF_PTHREAD_T);

_Static_assert(sizeof(struct _hajThreadBarrierAttr) == sizeof(pthread_barrierattr_t),
			   "_hajThreadBarrierAttr does not fit in pthread_barrierattr_t");
_Static_assert(_Alignof(struct _hajThreadBarrierAttr) == _Alignof(pthread_barrierattr_t),
			   "_hajThreadBarrierAttr alignment exceeds pthread_barrierattr_t");

# define HAJ_BARRIERATTR(p)			((struct _hajThreadBarrierAttr *)(void *)(p))
# define HAJ_BARRIERATTR_CONST(p)	((const struct _hajThreadBarrierAttr *)(const void *)(p))

#endif /* _BITS_THREAD_PTHREAD_ATTR_H */
