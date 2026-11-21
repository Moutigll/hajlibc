/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fstat.c
 * @brief Get file status from a file descriptor.
 * @Created: 2026/11/22 00:26:26 by Moutig
 * @Updated: 2026/11/22 00:29:24 by Moutig
 *
 * Thin wrapper over the fstat syscall.
 */

#include <sys/stat.h>
#include <errno.h>
#include <bits/syscall.h>

int	fstat(int fd, struct stat *st)
{
	long	r;

	r = __haj_syscall2(SYS_fstat, fd, (long)st);
	if (r < 0)
	{
		errno = (int)-r;
		return (-1);
	}
	return (0);
}
