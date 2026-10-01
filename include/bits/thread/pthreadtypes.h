/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file pthreadtypes.h
 * @brief Opaque pthread types, sizes and alignments.
 * @Created: 2026/10/01 10:53:08 by Moutig
 * @Updated: 2026/10/01 11:49:22 by Moutig
 *
 * All POSIX thread objects are opaque: user code only ever
 * manipulates pointers to them. This header defines:
 *
 *   - __SIZEOF_PTHREAD_*_T : the size in bytes of each object
 *     as seen by user code. These are ABI constants: they MUST
 *     NOT change within a major version of hajlibc.
 *
 *   - __ALIGNOF_PTHREAD_T : the alignment required by every
 *     pthread object.
 *
 *   - the typedefs themselves, as unions of a char buffer of
 *     exactly __SIZEOF_PTHREAD_*_T bytes and a `long` to force
 *     the alignment.
 *
 * The real internal layout of each object lives in a private
 * header (bits/thread/pthread_<name>.h), included only by the
 * implementation.
 *
 * Sizes are chosen to fit the current internal structures
 * exactly. If a structure grows, a _Static_assert in the
 * private header fails, and the size must be bumped (which is
 * an ABI change).
 *
 * Note on small objects: the union always contains a `long`,
 * so the minimum size and alignment of any object is that of
 * `long` (8 bytes on LP64, 4 on ILP32). Objects whose real
 * payload is smaller than that still occupy sizeof(long).
 *
 * hajlibc is NOT ABI-compatible with glibc or musl. Objects
 * from different libcs must not be mixed.
 */

#ifndef _BITS_THREAD_PTHREADTYPES_H
# define _BITS_THREAD_PTHREADTYPES_H

# include <bits/wordsize.h>

/* ----- Sizes (bytes) -----
 *
 * Every value must be >= sizeof(long). On LP64 that means >= 8.
 * The two smallest objects (barrierattr, spinlock) have a real
 * payload of 4 bytes, but the union's `long` member forces them
 * to 8. Do not try to shrink them below sizeof(long): the
 * typedef would not match.
 */

# define __SIZEOF_PTHREAD_ATTR_T			48
# define __SIZEOF_PTHREAD_MUTEX_T			32
# define __SIZEOF_PTHREAD_MUTEXATTR_T		16
# define __SIZEOF_PTHREAD_COND_T			32
# define __SIZEOF_PTHREAD_CONDATTR_T		8
# define __SIZEOF_PTHREAD_RWLOCK_T			40
# define __SIZEOF_PTHREAD_RWLOCKATTR_T		8
# define __SIZEOF_PTHREAD_BARRIER_T			32
# define __SIZEOF_PTHREAD_BARRIERATTR_T		8

/* ----- Alignment -----
 *
 * Every pthread object is aligned to `long`, which is the
 * natural alignment of the widest integer we use internally.
 * On LP64 this is 8 bytes, on ILP32 it is 4.
 */

# if __HAJ_WORDSIZE == 64
#  define __ALIGNOF_PTHREAD_T	8
# else
#  define __ALIGNOF_PTHREAD_T	4
# endif

/* ----- Type generator -----
 *
 * Defines a union whose size is exactly __SIZEOF_PTHREAD_<NAME>_T
 * bytes and whose alignment is that of `long`. The char array
 * holds the real data; the long forces the alignment.
 */

# define __HAJ_PTHREAD_TYPE(name) \
	union { \
		char __size[__SIZEOF_PTHREAD_##name##_T]; \
		long __align; \
	}

/* ----- Types ----- */

typedef __HAJ_PTHREAD_TYPE(ATTR)			pthread_attr_t;
typedef __HAJ_PTHREAD_TYPE(MUTEX)			pthread_mutex_t;
typedef __HAJ_PTHREAD_TYPE(MUTEXATTR)		pthread_mutexattr_t;
typedef __HAJ_PTHREAD_TYPE(COND)			pthread_cond_t;
typedef __HAJ_PTHREAD_TYPE(CONDATTR)		pthread_condattr_t;
typedef __HAJ_PTHREAD_TYPE(RWLOCK)			pthread_rwlock_t;
typedef __HAJ_PTHREAD_TYPE(RWLOCKATTR)		pthread_rwlockattr_t;
typedef __HAJ_PTHREAD_TYPE(BARRIER)			pthread_barrier_t;
typedef __HAJ_PTHREAD_TYPE(BARRIERATTR)		pthread_barrierattr_t;

/* ----- Sanity checks -----
 *
 * Every translation unit that includes this header verifies
 * that the typedefs match the size macros. If a macro and the
 * typedef diverge, the build fails immediately, everywhere.
 */

_Static_assert(sizeof(pthread_attr_t)			== __SIZEOF_PTHREAD_ATTR_T,
			  "pthread_attr_t size mismatch");
_Static_assert(_Alignof(pthread_attr_t)			== __ALIGNOF_PTHREAD_T,
			  "pthread_attr_t alignment mismatch");

_Static_assert(sizeof(pthread_mutex_t)			== __SIZEOF_PTHREAD_MUTEX_T,
			  "pthread_mutex_t size mismatch");
_Static_assert(_Alignof(pthread_mutex_t)		== __ALIGNOF_PTHREAD_T,
			  "pthread_mutex_t alignment mismatch");

_Static_assert(sizeof(pthread_mutexattr_t)		== __SIZEOF_PTHREAD_MUTEXATTR_T,
			  "pthread_mutexattr_t size mismatch");
_Static_assert(_Alignof(pthread_mutexattr_t)	== __ALIGNOF_PTHREAD_T,
			  "pthread_mutexattr_t alignment mismatch");

_Static_assert(sizeof(pthread_cond_t)			== __SIZEOF_PTHREAD_COND_T,
			  "pthread_cond_t size mismatch");
_Static_assert(_Alignof(pthread_cond_t)			== __ALIGNOF_PTHREAD_T,
			  "pthread_cond_t alignment mismatch");

_Static_assert(sizeof(pthread_condattr_t)		== __SIZEOF_PTHREAD_CONDATTR_T,
			  "pthread_condattr_t size mismatch");
_Static_assert(_Alignof(pthread_condattr_t)		== __ALIGNOF_PTHREAD_T,
			  "pthread_condattr_t alignment mismatch");

_Static_assert(sizeof(pthread_rwlock_t)			== __SIZEOF_PTHREAD_RWLOCK_T,
			  "pthread_rwlock_t size mismatch");
_Static_assert(_Alignof(pthread_rwlock_t)		== __ALIGNOF_PTHREAD_T,
			  "pthread_rwlock_t alignment mismatch");

_Static_assert(sizeof(pthread_rwlockattr_t)		== __SIZEOF_PTHREAD_RWLOCKATTR_T,
			  "pthread_rwlockattr_t size mismatch");
_Static_assert(_Alignof(pthread_rwlockattr_t)	== __ALIGNOF_PTHREAD_T,
			  "pthread_rwlockattr_t alignment mismatch");

_Static_assert(sizeof(pthread_barrier_t)		== __SIZEOF_PTHREAD_BARRIER_T,
			  "pthread_barrier_t size mismatch");
_Static_assert(_Alignof(pthread_barrier_t)		== __ALIGNOF_PTHREAD_T,
			  "pthread_barrier_t alignment mismatch");

_Static_assert(sizeof(pthread_barrierattr_t)	== __SIZEOF_PTHREAD_BARRIERATTR_T,
			  "pthread_barrierattr_t size mismatch");
_Static_assert(_Alignof(pthread_barrierattr_t)	== __ALIGNOF_PTHREAD_T,
			  "pthread_barrierattr_t alignment mismatch");

#endif /* _BITS_THREAD_PTHREADTYPES_H */
