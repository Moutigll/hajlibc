/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file gettid.c
 * @brief Implementation of __haj_gettid().
 * @Created: 2026/09/30 11:22:15 by Moutig
 * @Updated: 2026/10/03 16:14:59 by Moutig
 *
 * On Linux, gettid(2) returns the kernel thread ID, which is
 * distinct from the process ID. It is used internally by the
 * thread runtime to identify threads and to compare them.
 *
 * FreeBSD does not have gettid(2). It has thr_self(2), which
 * returns a thread identifier that is not exactly the same as
 * Linux's TID but serves the same purpose.
 *
 * Darwin has thread_selfid(2), which returns a 64-bit thread
 * identifier.
 *
 * This function is internal and is not part of POSIX.
 */

#include <bits/thread/thread.h>
#include <bits/syscall.h>
#include <bits/os.h>
#include <errno.h>

#if defined(HAJ_OS_LINUX)

int __haj_gettid(void)
{
	long r = __haj_syscall0(SYS_gettid);
	return ((int)r);
}

#elif defined(HAJ_OS_FREEBSD)

int __haj_gettid(void)
{
	long r = __haj_syscall0(SYS_thr_self);
	return ((int)r);
}

#elif defined(HAJ_OS_DARWIN)

int __haj_gettid(void)
{
	long r = __haj_syscall0(SYS_thread_selfid);
	return ((int)r);
}

#else

# error "hajlib: __haj_gettid is not implemented for this platform"

#endif

pid_t __haj_gettid_thread(pthread_t thread)
{
	struct __haj_tcb	*tcb;
	int					tid;

	if (thread == (pthread_t)0) {
		errno = ESRCH;
		return (-1);
	}

	tcb = (struct __haj_tcb *)(void *)thread;
	tid = tcb->tid;
	if (tid <= 0) {
		errno = ESRCH;
		return (-1);
	}
	return ((pid_t)tid);
}
