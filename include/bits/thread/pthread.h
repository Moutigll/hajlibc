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
 * @Updated: 2026/10/02 12:20:39 by Moutig
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
_Static_assert(_Alignof(struct _hajThreadAttr) <= _Alignof(pthread_attr_t),
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

struct _hajThreadMutex {
	int	lock;	/* futex word: 0 = free, 1 = locked, 2 = contended */
	int	type;	/* HAJ_MUTEX_NORMAL / RECURSIVE / ERRORCHECK */
	int	owner;	/* TID of the owning thread */
	int	count;	/* recursive lock count */
	int	robust;	/* non-zero if robust */
} __HAJ_ALIGNED(__ALIGNOF_PTHREAD_T);

_Static_assert(sizeof(struct _hajThreadMutex) == sizeof(pthread_mutex_t),
			   "_hajThreadMutex does not fit in pthread_mutex_t "
			   "(bump __SIZEOF_PTHREAD_MUTEX_T)");
_Static_assert(_Alignof(struct _hajThreadMutex) <= _Alignof(pthread_mutex_t),
			   "_hajThreadMutex alignment exceeds pthread_mutex_t "
			   "(bump __ALIGNOF_PTHREAD_MUTEX_T)");

# define HAJ_MUTEX(p) \
	((struct _hajThreadMutex *)(void *)(p))
# define HAJ_MUTEX_CONST(p) \
	((const struct _hajThreadMutex *)(const void *)(p))

/* ----- Mutex attributes ----- */

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
_Static_assert(_Alignof(struct _hajThreadMutexAttr) <= _Alignof(pthread_mutexattr_t),
			   "_hajThreadMutexAttr alignment exceeds pthread_mutexattr_t "
			   "(bump __ALIGNOF_PTHREAD_T)");

# define HAJ_MUTEXATTR(p) \
	((struct _hajThreadMutexAttr *)(void *)(p))
# define HAJ_MUTEXATTR_CONST(p) \
	((const struct _hajThreadMutexAttr *)(const void *)(p))

#endif /* _BITS_THREAD_PTHREAD_ATTR_H */
