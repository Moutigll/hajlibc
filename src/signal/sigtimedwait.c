/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file sigtimedwait.c
 * @brief Implementation of sigtimedwait() and sigwaitinfo().
 * @Created: 2026/10/03 16:07:25 by Moutig
 * @Updated: 2026/10/03 16:09:33 by Moutig
 *
 * sigtimedwait() suspends execution of the calling thread until
 * one of the signals specified in set is pending for the process or
 * until the time interval specified by timeout expires. If a signal is pending,
 * it is removed from the set of pending signals and its information is returned in info.
 * sigwaitinfo() is equivalent to sigtimedwait() with a NULL timeout.
 */


#include <signal.h>
#include <errno.h>
#include <string.h>
#include <bits/syscall.h>
#include <bits/signal.h>

int sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout)
{
	unsigned long	mask;
	siginfo_t	si;
	long		r;

	if (set == NULL) {
		errno = EINVAL;
		return (-1);
	}
	mask = set->__bits[0];

	memset(&si, 0, sizeof(si));
	r = __haj_syscall4(SYS_rt_sigtimedwait,
					   (long)&mask,
					   (long)&si,
					   (long)timeout,
					   8);	/* sizeof(kernel sigset_t) */
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	if (info != NULL)
		*info = si;
	return (si.si_signo);
}

int sigwaitinfo(const sigset_t *set, siginfo_t *info)
{
	return (sigtimedwait(set, info, NULL));
}
