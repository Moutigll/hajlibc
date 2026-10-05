/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file getcpuclockid.c
 * @brief Implementation of pthread_getcpuclockid().
 * @Created: 2026/10/05 11:12:07 by Moutig
 * @Updated: 2026/10/05 11:13:29 by Moutig
 *
 * pthread_getcpuclockid() retrieves the clock ID associated with
 * a specific thread. This clock ID can be used with clock_gettime()
 * to obtain CPU time consumed by the thread. The function returns
 * 0 on success and sets the clock ID in the provided pointer. If the
 * thread is invalid, it returns ESRCH. If the clock ID pointer is NULL,
 * it returns EINVAL.
 */

#include <pthread.h>
#include <errno.h>
#include <bits/thread/thread.h>

# define HAJ_CPUCLOCK_SCHED				2
# define HAJ_CPUCLOCK_PERTHREAD_MASK	4
# define HAJ_MAKE_THREAD_CPUCLOCK(tid, clock) \
	((~(clockid_t)(tid) << 3) | (clockid_t)((clock) | HAJ_CPUCLOCK_PERTHREAD_MASK))

int pthread_getcpuclockid(pthread_t thread, clockid_t *clockid)
{
	pid_t	tid;

	if (clockid == NULL)
		return (EINVAL);

	tid = __haj_gettid_thread(thread);
	if (tid < 0)
		return (ESRCH);

	*clockid = HAJ_MAKE_THREAD_CPUCLOCK(tid, HAJ_CPUCLOCK_SCHED);
	return (0);
}
