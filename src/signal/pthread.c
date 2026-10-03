/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file pthread.c
 * @brief Implementation of pthread_kill() and pthread_sigmask().
 * @Created: 2026/10/03 16:10:12 by Moutig
 * @Updated: 2026/10/03 16:30:25 by Moutig
 *
 * pthread_kill() sends a signal to a specific thread in the same process.
 * pthread_sigmask() is identical to sigprocmask() in a multithreaded
 * process, as the kernel signal mask is per-thread on Linux.
 */

#include <signal.h>
#include <errno.h>
#include <bits/syscall.h>
#include <bits/thread/thread.h>

int pthread_kill(pthread_t thread, int signum)
{
	long	tgid;
	long	tid;
	long	r;

	tgid	= __haj_syscall0(SYS_getpid);
	tid		= __haj_gettid_thread(thread);
	if (tid < 0) {
		errno = (int)-tid;
		return (-1);
	}

	r = __haj_syscall3(SYS_tgkill, tgid, tid, (long)signum);
	if (r < 0)
		return ((int)-r);
	return (0);
}

int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
	/*
	 * Sur Linux, le masque de signaux est déjà par-thread :
	 * rt_sigprocmask s'applique au thread appelant.
	 */
	if (sigprocmask(how, set, oldset) != 0)
		return (errno);
	return (0);
}

int sigwait(const sigset_t *set, int *sig)
{
	siginfo_t	si;
	int		r;

	if (set == NULL || sig == NULL)
		return (EINVAL);
	r = sigwaitinfo(set, &si);
	if (r < 0)
		return (errno);
	*sig = si.si_signo;
	return (0);
}
