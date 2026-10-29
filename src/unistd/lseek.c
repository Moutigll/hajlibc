/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file lseek.c
 * @brief Implementation of the lseek system call.
 * @Created: 2026/10/27 02:28:02 by Moutig
 * @Updated: 2026/10/27 02:28:39 by Moutig
 *
 * This file contains the implementation of the lseek system call,
 * which repositions the offset of the file descriptor.
 */

#include <errno.h>
#include <unistd.h>
#include <bits/syscall.h>

off_t lseek(int fd, off_t offset, int whence) {
	long r;

	r = __haj_syscall3(SYS_lseek, fd, (long)offset, (long)whence);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return ((off_t)r);
}
