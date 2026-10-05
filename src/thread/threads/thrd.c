/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file thrd.c
 * @brief Implementation of C11 threads functions.
 * @Created: 2026/10/05 11:28:49 by Moutig
 * @Updated: 2026/10/05 13:16:03 by Moutig
 *
 * This file implements the C11 threads API, providing functions
 * for creating and managing threads, as well as thread-specific
 * data and synchronization primitives. The implementation is built
 * on top of the POSIX threads (pthreads) library, translating
 * between C11 and POSIX error codes and types where necessary.
 */

#include <stdint.h>
#include <threads.h>
#include <pthread.h>
#include <errno.h>

static int hajThrdStatus(int pth)
{
	switch (pth) {
	case 0:			return (thrd_success);
	case ENOMEM:	return (thrd_nomem);
	case EBUSY:		return (thrd_busy);
	case ETIMEDOUT:	return (thrd_timedout);
	default:		return (thrd_error);
	}
}

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
	pthread_t	t;
	int			r;

	r = pthread_create(&t, NULL, (void *(*)(void *))(uintptr_t)func, arg);
	if (r != 0)
		return (hajThrdStatus(r));
	*thr = t;
	return (thrd_success);
}

thrd_t thrd_current(void)
{
	return (pthread_self());
}

int thrd_equal(thrd_t lhs, thrd_t rhs)
{
	return (pthread_equal(lhs, rhs));
}

int thrd_sleep(const struct timespec *duration, struct timespec *remaining)
{
	return (nanosleep(duration, remaining));
}

void thrd_yield(void)
{
	sched_yield();
}

int thrd_detach(thrd_t thr)
{
	return (hajThrdStatus(pthread_detach(thr)));
}

int thrd_join(thrd_t thr, int *res)
{
	void	*ret = NULL;
	int		r;

	r = pthread_join(thr, &ret);
	if (r != 0)
		return (hajThrdStatus(r));
	if (res != NULL)
		*res = (int)(long)ret;
	return (thrd_success);
}

void thrd_exit(int res)
{
	pthread_exit((void *)(long)res);
}
