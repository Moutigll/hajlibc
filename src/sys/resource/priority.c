/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file priority.c
 * @brief Implementation of getpriority() and setpriority().
 * @Created: 2026/09/30 13:16:16 by Moutig
 * @Updated: 2026/09/30 13:40:15 by Moutig
 *
 * getpriority() and setpriority() are implemented via the
 * SYS_getpriority and SYS_setpriority syscalls. The kernel returns
 * 20 - nice_value on success, and -errno on error. We handle this
 * by returning 20 - syscall_result and setting errno on error.
 */

#include <sys/resource.h>
#include <errno.h>
#include <bits/syscall.h>

int getpriority(int which, id_t who)
{
	long r = __haj_syscall2(SYS_getpriority, which, (long)who);

	if (r < 0 && r >= -4095) {
		errno = (int)-r;
		return (-1);
	}
	return ((int)(20 - r));
}
int setpriority(int which, id_t who, int prio)
{
	long r;

	r = __haj_syscall3(SYS_setpriority, which, (long)who, (long)prio);
	if (r < 0 && r >= -4095) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}
