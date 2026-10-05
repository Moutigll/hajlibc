/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file sched.c
 * @brief Scheduling syscalls.
 * @Created: 2026/10/05 11:37:03 by Moutig
 * @Updated: 2026/10/05 11:48:59 by Moutig
 *
 * Thin wrappers around the scheduler syscalls. On Linux, the
 * sched_*(2) family takes a PID or TID as first argument; the
 * kernel resolves it to a task_struct. Passing a TID works
 * because Linux threads are tasks.
 */

#include <stddef.h>
#include <sched.h>
#include <errno.h>
#include <bits/syscall.h>

#if defined(HAJ_OS_LINUX)

int sched_yield(void)
{
	long	r;

	r = __haj_syscall0(SYS_sched_yield);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

int sched_getscheduler(pid_t pid)
{
	long	r;

	r = __haj_syscall1(SYS_sched_getscheduler, (long)pid);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return ((int)r);
}

int sched_setscheduler(pid_t pid, int policy, const struct sched_param *param)
{
	long	r;

	r = __haj_syscall3(SYS_sched_setscheduler, (long)pid, (long)policy, (long)param);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

int sched_getparam(pid_t pid, struct sched_param *param)
{
	long	r;

	if (param == NULL) {
		errno = EINVAL;
		return (-1);
	}
	r = __haj_syscall2(SYS_sched_getparam, (long)pid, (long)param);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

int sched_setparam(pid_t pid, const struct sched_param *param)
{
	long	r;

	if (param == NULL) {
		errno = EINVAL;
		return (-1);
	}
	r = __haj_syscall2(SYS_sched_setparam, (long)pid, (long)param);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

int sched_get_priority_min(int policy)
{
	long	r;

	r = __haj_syscall1(SYS_sched_get_priority_min, (long)policy);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return ((int)r);
}

int sched_get_priority_max(int policy)
{
	long	r;

	r = __haj_syscall1(SYS_sched_get_priority_max, (long)policy);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return ((int)r);
}

int sched_rr_get_interval(pid_t pid, struct timespec *interval)
{
	long	r;

	if (interval == NULL) {
		errno = EINVAL;
		return (-1);
	}
	r = __haj_syscall2(SYS_sched_rr_get_interval, (long)pid,
					   (long)interval);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask)
{
	long	r;

	if (mask == NULL) {
		errno = EINVAL;
		return (-1);
	}
	r = __haj_syscall3(SYS_sched_getaffinity,
					   (long)pid, (long)cpusetsize, (long)mask);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask)
{
	long	r;

	if (mask == NULL) {
		errno = EINVAL;
		return (-1);
	}
	r = __haj_syscall3(SYS_sched_setaffinity,
					   (long)pid, (long)cpusetsize, (long)mask);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

#else
# error "sched.c: only Linux is supported by this file"
#endif
