/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file getpid.c
 * @brief Implementation of getpid().
 * @Created: 2026/09/28 11:44:15 by Moutig
 * @Updated: 2026/09/30 09:20:15 by Moutig
 *
 * Returns the process ID of the calling process. The PID is
 * placed in the parent process ID field of the child for
 * fork(), and can be obtained from getppid() in the child.
 */

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <bits/syscall.h>

pid_t getpid(void) {
	long ret;

	ret = __haj_syscall0(SYS_getpid);
	if (ret < 0 && ret >= -4095) {
		errno = (int)-ret;
		return (-1);
	}
	return ((pid_t)ret);
}
