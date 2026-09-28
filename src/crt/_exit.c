/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file _exit.c
 * @brief Implementation of _exit().
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/26 05:04:17 by Moutig
 *
 * _exit() terminates the process immediately, without running
 * atexit handlers or flushing stdio buffers. This is the
 * "raw" termination, used by exit() after it has done its job.
 *
 * On Linux, we use the exit_group syscall, which terminates all
 * threads in the process. On other OSes, we use the appropriate
 * exit syscall.
 */

#include <unistd.h>
#include <bits/syscall.h>

__HAJ_NORETURN
void	_exit(int status)
{
#if defined(HAJ_OS_LINUX)
	/*
	 * exit_group terminates all threads, not just the calling
	 * one. This is the correct behavior for a process-wide
	 * exit.
	 */
	__haj_syscall1(SYS_exit_group, status);
#else
	/*
	 * On other OSes, exit terminates the process. There is no
	 * distinction between "exit this thread" and "exit the
	 * process" in the base syscall.
	 */
	__haj_syscall1(SYS_exit, status);
#endif

	/*
	 * The syscall should never return. If it does (e.g. if the
	 * syscall number is wrong), we loop forever to avoid
	 * undefined behavior.
	 */
	for (;;) {
	}
}
