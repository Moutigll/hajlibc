/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file cnd.c
 * @brief C11 condition variables.
 * @Created: 2026/10/05 11:33:24 by Moutig
 * @Updated: 2026/10/05 12:57:17 by Moutig
 *
 * cnd_t is pthread_cond_t, so every function is a direct
 * wrapper. The return values are converted to thrd_t values.
 */

#include <threads.h>
#include <pthread.h>
#include <errno.h>

_Static_assert(sizeof(cnd_t) >= sizeof(pthread_cond_t),
			   "cnd_t is too small for pthread_cond_t");
static int hajCndStatus(int pth)
{
	switch (pth) {
	case 0:			return (thrd_success);
	case ENOMEM:	return (thrd_nomem);
	case EBUSY:		return (thrd_busy);
	case ETIMEDOUT:	return (thrd_timedout);
	default:		return (thrd_error);
	}
}

int cnd_init(cnd_t *cond)
{
	if (cond == NULL)
		return (thrd_error);

	return (hajCndStatus(pthread_cond_init(cond, NULL)));
}

int cnd_signal(cnd_t *cond)
{

	if (cond == NULL)
		return (thrd_error);
	return (hajCndStatus(pthread_cond_signal(cond)));
}

int cnd_broadcast(cnd_t *cond)
{
	if (cond == NULL)
		return (thrd_error);
	return (hajCndStatus(pthread_cond_broadcast(cond)));
}

int cnd_wait(cnd_t *cond, mtx_t *mtx)
{
	if (cond == NULL || mtx == NULL)
		return (thrd_error);
	return (hajCndStatus(pthread_cond_wait(cond, mtx)));
}

int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *ts)
{
	if (cond == NULL || mtx == NULL || ts == NULL)
		return (thrd_error);
	return (hajCndStatus(pthread_cond_timedwait(cond, mtx, ts)));
}

void cnd_destroy(cnd_t *cond)
{
	if (cond == NULL)
		return;
	pthread_cond_destroy(cond);
}
