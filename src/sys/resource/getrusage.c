/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file getrusage.c
 * @brief Implementation of getrusage().
 * @Created: 2026/09/30 13:15:56 by Moutig
 * @Updated: 2026/09/30 13:39:30 by Moutig
 *
 * getrusage() retrieves resource usage statistics for the calling process.
 */

#include <sys/resource.h>
#include <errno.h>
#include <bits/syscall.h>

int getrusage(int who, struct rusage *usage)
{
	long r;

	if (usage == NULL) {
		errno = EFAULT;
		return (-1);
	}

	r = __haj_syscall2(SYS_getrusage, who, (long)usage);
	if (r < 0 && r >= -4095) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}
