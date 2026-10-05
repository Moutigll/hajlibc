/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file raise.c
 * @brief Implementation of raise() and related functions.
 * @Created: 2026/10/03 14:57:24 by Moutig
 * @Updated: 2026/10/03 16:03:52 by Moutig
 *
 * raise() in a multithreaded process must send the signal to
 * the calling thread only. On Linux, that is tgkill(getpid(),
 * gettid(), signum). kill(getpid(), ...) would target the
 * process as a whole and could deliver the signal to another
 * thread.
 */

#include "unistd.h"
#include <signal.h>
#include <errno.h>
#include <bits/syscall.h>
#include <bits/thread/thread.h>
#include <string.h>

int raise(int signum)
{
	long	tgid;
	long	tid;
	long	r;

	tgid = __haj_syscall0(SYS_getpid);
	tid = __haj_gettid();
	if (tgid < 0 || tid < 0) {
		errno = (int)((tgid < 0) ? -tgid : -tid);
		return (-1);
	}

	r = __haj_syscall3(SYS_tgkill, tgid, tid, (long)signum);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

int kill(pid_t pid, int signum)
{
	long	r;

	r = __haj_syscall2(SYS_kill, (long)pid, (long)signum);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

int tgkill(pid_t tgid, pid_t tid, int signum)
{
	long	r;

	r = __haj_syscall3(SYS_tgkill, (long)tgid, (long)tid, (long)signum);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}

int killpg(pid_t pgrp, int signum)
{
	if (pgrp < 0) {
		errno = EINVAL;
		return (-1);
	}
	return (kill(-pgrp, signum));
}

int sigqueue(pid_t pid, int signum, const union sigval value)
{
	siginfo_t	si;
	long		r;

	memset(&si, 0, sizeof(si));
	si.si_signo = signum;
	si.si_code	= SI_QUEUE;
	si.si_pid	= getpid();
	si.si_uid	= getuid();
	si.si_value	= value;

	r = __haj_syscall3(SYS_rt_sigqueueinfo,
					   (long)pid, (long)signum, (long)&si);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}
