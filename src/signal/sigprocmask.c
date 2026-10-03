/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file sigprocmask.c
 * @brief Implementation of sigprocmask(), sigpending() and sigsuspend().
 * @Created: 2026/10/03 14:56:31 by Moutig
 * @Updated: 2026/10/03 15:33:40 by Moutig
 *
 * sigprocmask() and sigpending() are implemented via the
 * rt_sigprocmask() and rt_sigpending() syscalls. sigsuspend()
 * is implemented via the rt_sigsuspend() syscall. The kernel
 * expects the size of the kernel sigset_t as the 4th argument,
 * which is 8 bytes on 64-bit. Our sigset_t has the same layout
 */

#include <signal.h>
#include <errno.h>
#include <bits/syscall.h>

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
	unsigned long	newmask = set ? set->__bits[0] : 0;
	unsigned long	oldmask = 0;
	long			r;

	/*
	 * rt_sigprocmask(how, set, oldset, sigsetsize). The kernel
	 * copies exactly sigsetsize bytes; we pass 8 for 64-bit.
	 */
	r = __haj_syscall4(SYS_rt_sigprocmask,
					   (long)how,
					   set ? (long)&newmask : 0,
					   oldset ? (long)&oldmask : 0,
					   8);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	if (oldset != NULL)
		oldset->__bits[0] = oldmask;
	return (0);
}

int sigpending(sigset_t *set)
{
	unsigned long	pending = 0;
	long			r;

	if (set == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/*
	 * rt_sigpending(set, sigsetsize). The "set" returned is the
	 * union of the per-thread pending set and the per-process
	 * pending set for signals that are blocked.
	 */
	r = __haj_syscall2(SYS_rt_sigpending,
					   (long)&pending,
					   8);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	set->__bits[0] = pending;
	return (0);
}

int sigsuspend(const sigset_t *mask)
{
	unsigned long	m;
	long			r;

	if (mask == NULL) {
		errno = EINVAL;
		return (-1);
	}
	m = mask->__bits[0];

	/*
	 * rt_sigsuspend(mask, sigsetsize). Always returns -EINTR.
	 */
	r = __haj_syscall2(SYS_rt_sigsuspend, (long)&m, 8);
	errno = (r < 0) ? (int)-r : EINTR;
	return (-1);
}
