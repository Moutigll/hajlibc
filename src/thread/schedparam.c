/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file schedparam.c
 * @brief Implementation of pthread_getschedparam(), pthread_setschedparam() and pthread_setschedprio().
 * @Created: 2026/10/05 11:14:12 by Moutig
 * @Updated: 2026/10/05 12:59:14 by Moutig
 *
 * This file implements the functions for getting and setting
 * the scheduling parameters of threads, including their scheduling
 * policy and priority. These functions allow applications to query
 * and modify the scheduling behavior of threads in a POSIX-compliant manner.
 *
 * The pthread_* wrappers return a POSIX error code (0 on
 * success), while the underlying sched_* functions return -1
 * and set errno. We translate between the two.
 */

#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <bits/thread/thread.h>

int pthread_getschedparam(pthread_t thread, int *policy, struct sched_param *param)
{
	pid_t	tid;
	int		r;

	if (policy == NULL || param == NULL)
		return (EINVAL);

	tid = __haj_gettid_thread(thread);
	if (tid < 0)
		return (errno);

	r = sched_getscheduler(tid);
	if (r < 0)
		return (errno);
	*policy = r;

	if (sched_getparam(tid, param) != 0)
		return (errno);
	return (0);
}

int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param *param)
{
	pid_t	tid;

	if (param == NULL)
		return (EINVAL);

	tid = __haj_gettid_thread(thread);
	if (tid < 0)
		return (errno);

	if (sched_setscheduler(tid, policy, param) != 0)
		return (errno);
	return (0);
}

int pthread_setschedprio(pthread_t thread, int prio)
{
	pid_t				tid;
	struct sched_param	param;

	tid = __haj_gettid_thread(thread);
	if (tid < 0)
		return (errno);

	param.sched_priority = prio;
	if (sched_setparam(tid, &param) != 0)
		return (errno);
	return (0);
}
