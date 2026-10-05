/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fork.c
 * @brief Implementation of fork().
 * @Created: 2026/10/01 08:39:40 by Moutig
 * @Updated: 2026/10/05 11:10:47 by Moutig
 *
 * fork() creates a new process that is a copy of the calling
 * process. The child gets a copy of the address space
 * (copy-on-write), the file descriptor table, the signal
 * handlers, etc. The child has a single thread: the one that
 * called fork().
 *
 * On Linux x86_64, FreeBSD and Darwin, fork is a direct syscall.
 * On Linux aarch64, there is no SYS_fork; fork is implemented
 * with clone(SIGCHLD, ...), which is what glibc and musl do.
 *
 * The aarch64 path uses inline assembly because C cannot easily
 * control all the argument registers of the clone syscall.
 */

#include <unistd.h>
#include <errno.h>
#include <bits/syscall.h>
#include <bits/os.h>
#include <bits/thread/thread.h>

#if defined(HAJ_OS_LINUX) && defined(HAJ_ARCH_AARCH64)

/*
 * aarch64 has no SYS_fork. We implement fork() with
 * clone(SIGCHLD, stack=0), which is equivalent to fork:
 *   - no CLONE_VM, so the child gets a copy of the address space
 *   - no CLONE_THREAD, so the child is a separate process
 *   - SIGCHLD is the exit signal sent to the parent
 */
static pid_t hajForkAarch64(void)
{
	register long x0 __asm__("x0") = 17;	/* SIGCHLD */
	register long x1 __asm__("x1") = 0;		/* stack */
	register long x2 __asm__("x2") = 0;		/* parent_tid */
	register long x3 __asm__("x3") = 0;		/* tls */
	register long x4 __asm__("x4") = 0;		/* child_tid */
	register long x8 __asm__("x8") = 220;	/* SYS_clone */

	__asm__ volatile (
		"svc #0"
		: "+r" (x0)
		: "r" (x1), "r" (x2), "r" (x3), "r" (x4), "r" (x8)
		: "memory", "cc"
	);

	return ((pid_t)x0);
}

#endif /* HAJ_OS_LINUX && HAJ_ARCH_AARCH64 */

pid_t fork(void)
{
	pid_t	pid;

	__haj_atforkPrepare();

#if defined(HAJ_OS_LINUX) && defined(HAJ_ARCH_AARCH64)
	pid = hajForkAarch64();
#else
	pid = (pid_t)__haj_syscall0(SYS_fork);
#endif

	if (pid < 0) {
		__haj_atforkParent();
		errno = (int)-pid;
		return (-1);
	}
	if (pid == 0) {
		struct __haj_tcb *tcb = __haj_tcbSelf();

#if HAJ_PTHREAD_PROCESS_SHARED
		__haj_robustInit(&tcb->robustList);
#endif
		tcb->tid = __haj_gettid();
		__haj_threadListReset(tcb);
		__haj_atforkChild();
	} else
		__haj_atforkParent();
	return (pid);
}
