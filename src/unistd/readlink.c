/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file readlink.c
 * @brief readlink(2) — read the target of a symbolic link.
 * @Created: 2026/10/29 05:37:47 by Moutig
 * @Updated: 2026/10/29 05:38:28 by Moutig
 *
 * Wraps the raw syscall. The kernel does not NUL-terminate the
 * buffer; the caller must do it using the returned length, and
 * must size `buf` at one more than the maximum link length it
 * expects (PATH_MAX by convention).
 */

#include <unistd.h>
#include <errno.h>

ssize_t readlink(const char *path, char *buf, size_t bufsiz)
{
	long r = __haj_syscall3(SYS_readlink, (long)path, (long)buf, (long)bufsiz);

	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return ((ssize_t)r);
}
