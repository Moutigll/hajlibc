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
 * @Updated: 2026/10/01 11:51:27 by Moutig
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

/**
 * @brief Internal representation of pthread_attr_t.
 *
 * This struct represents the internal layout of a pthread
 * attribute object. It contains fields for stack size, guard
 * size, stack address, detach state, scheduling policy,
 * scheduling priority, inherit scheduler setting, and scope.
 */
struct haj_attr_internal {
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
};

/* The internal struct must fit in the public opaque type. */
_Static_assert(sizeof(struct haj_attr_internal) == sizeof(pthread_attr_t),
			   "haj_attr_internal does not fit in pthread_attr_t "
			   "(bump __SIZEOF_PTHREAD_ATTR_T)");
_Static_assert(_Alignof(struct haj_attr_internal) <= _Alignof(pthread_attr_t),
			   "haj_attr_internal alignment exceeds pthread_attr_t "
			   "(bump __ALIGNOF_PTHREAD_T)");

/**
 * @brief Cast a public pthread_attr_t to its internal struct.
 *
 * The public type is a union containing a char buffer; casting
 * to a struct pointer is legal and the only field access path.
 */
# define HAJ_ATTR(p) \
	((struct haj_attr_internal *)(void *)(p))
# define HAJ_ATTR_CONST(p) \
	((const struct haj_attr_internal *)(const void *)(p))

#endif /* _BITS_THREAD_PTHREAD_ATTR_H */
