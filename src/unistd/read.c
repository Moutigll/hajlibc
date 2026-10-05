/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file read.c
 * @brief Implementation of read().
 * @Created: 2026/10/01 08:39:16 by Moutig
 * @Updated: 2026/10/01 08:39:27 by Moutig
 *
 * read() reads up to n bytes from the file descriptor fd into
 * the buffer buf. It returns the number of bytes actually read
 * (which may be less than n), 0 at end of file, or -1 on error.
 *
 * This is a thin wrapper around the read(2) syscall.
 */

#include <unistd.h>
#include <errno.h>
#include <bits/syscall.h>

ssize_t read(int fd, void *buf, size_t n)
{
	long r;

	r = __haj_syscall3(SYS_read, fd, (long)buf, (long)n);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return ((ssize_t)r);
}
