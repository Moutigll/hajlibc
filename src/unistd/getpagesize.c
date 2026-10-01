/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file getpagesize.c
 * @brief BSD getpagesize() implementation.
 * @Created: 2026/09/30 13:02:47 by Moutig
 * @Updated: 2026/10/01 08:59:10 by Moutig
 *
 * getpagesize() is not POSIX. It is a BSD extension kept for
 * compatibility with legacy code. Modern code should call
 * sysconf(_SC_PAGESIZE) instead.
 *
 * The implementation is a thin wrapper over sysconf().
 */

#include <unistd.h>
#include <errno.h>

#if __HAJ_SOURCE

int getpagesize(void)
{
	long ps = sysconf(_SC_PAGESIZE);

	if (ps <= 0) {
		/* sysconf() may set errno; keep it. */
		if (errno == 0)
			errno = EINVAL;
		return (-1);
	}

	return ((int)ps);
}

#endif /* __HAJ_SOURCE */
