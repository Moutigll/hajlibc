/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file dup2.c
 * @brief Implementation of the dup2 system call.
 * @Created: 2026/10/27 02:19:02 by Moutig
 * @Updated: 2026/11/12 13:33:19 by Moutig
 *
 * This file contains the implementation of the dup2 system call,
 * which duplicates a file descriptor.
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <bits/syscall.h>

int dup2(int oldfd, int newfd) {
	long r;

	if (oldfd == newfd) {
	/* dup2 must fail with EBADF if oldfd is invalid, even when oldfd == newfd.*/
		if (__haj_syscall2(SYS_fcntl, oldfd, F_GETFD) < 0)
			return (-1);
		return (newfd);
	}

	r = __haj_syscall3(SYS_dup3, oldfd, newfd, 0);
	if (r < 0) {
		errno = ((int)-r);
		return (-1);
	}
	return ((int)r);
}
