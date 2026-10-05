/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file mtx.c
 * @brief C11 mutexes.
 * @Created: 2026/10/05 11:33:13 by Moutig
 * @Updated: 2026/10/05 12:57:30 by Moutig
 *
 * C11 mutexes have three flavours: plain (non-recursive),
 * recursive, and timed (plain + timedlock). We translate them
 * to the corresponding pthread_mutexattr_t type.
 */

#include <threads.h>
#include <pthread.h>
#include <errno.h>

_Static_assert(sizeof(mtx_t) >= sizeof(pthread_mutex_t),
			   "mtx_t is too small for pthread_mutex_t");

static int hajMtxStatus(int pth)
{
	switch (pth) {
	case 0:			return (thrd_success);
	case ENOMEM:	return (thrd_nomem);
	case EBUSY:		return (thrd_busy);
	case ETIMEDOUT:	return (thrd_timedout);
	default:		return (thrd_error);
	}
}

int mtx_init(mtx_t *mtx, int type)
{
	pthread_mutexattr_t	attr;
	int					pthType;
	int					r;

	if (mtx == NULL)
		return (thrd_error);
	if (type & ~(mtx_plain | mtx_recursive | mtx_timed))
		return (thrd_error);

	/*
	 * mtx_timed is accepted but has no effect on the underlying
	 * mutex: POSIX does not have a "timed-only" mutex type. The
	 * type only changes which operations are allowed.
	 */
	pthType = (type & mtx_recursive)
		? PTHREAD_MUTEX_RECURSIVE
		: PTHREAD_MUTEX_NORMAL;

	r = pthread_mutexattr_init(&attr);
	if (r != 0)
		return (hajMtxStatus(r));
	r = pthread_mutexattr_settype(&attr, pthType);
	if (r != 0) {
		pthread_mutexattr_destroy(&attr);
		return (hajMtxStatus(r));
	}
	r = pthread_mutex_init(mtx, &attr);
	pthread_mutexattr_destroy(&attr);
	if (r != 0)
		return (hajMtxStatus(r));
	return (thrd_success);
}

int mtx_lock(mtx_t *mtx)
{
	if (mtx == NULL)
		return (thrd_error);
	return (hajMtxStatus(pthread_mutex_lock(mtx)));
}

int mtx_trylock(mtx_t *mtx)
{
	if (mtx == NULL)
		return (thrd_error);
	return (hajMtxStatus(pthread_mutex_trylock(mtx)));
}

int mtx_timedlock(mtx_t *mtx, const struct timespec *ts)
{
	if (mtx == NULL || ts == NULL)
		return (thrd_error);
	return (hajMtxStatus(pthread_mutex_timedlock(mtx, ts)));
}

int mtx_unlock(mtx_t *mtx)
{
	if (mtx == NULL)
		return (thrd_error);
	return (hajMtxStatus(pthread_mutex_unlock(mtx)));
}

void mtx_destroy(mtx_t *mtx)
{
	if (mtx == NULL)
		return;
	pthread_mutex_destroy(mtx);
}
