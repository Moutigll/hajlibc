/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file write.c
 * @brief Implementation of the write() function.
 * @Created: 2026/09/30 08:53:43 by Moutig
 * @Updated: 2026/09/30 11:02:33 by Moutig
 *
 * This file contains the implementation of the write() function,
 * which is used to write data to a file descriptor.
 * It utilizes the system call interface to perform the write operation and
 * handles error reporting through the errno variable.
 */

#include <errno.h>
#include <unistd.h>
#include <bits/syscall.h>

ssize_t write(int fd, const void *buf, size_t n)
{
	long r;

	r = __haj_syscall3(SYS_write, fd, (long)buf, (long)n);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return ((ssize_t)r);
}
