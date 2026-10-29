/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file pipe.c
 * @brief Implementation of the pipe system call.
 * @Created: 2026/10/27 02:19:01 by Moutig
 * @Updated: 2026/10/27 02:24:37 by Moutig
 *
 * This file contains the implementation of the pipe system call,
 * which creates a unidirectional data channel that can be used for
 * interprocess communication.
 */

#include <errno.h>
#include <unistd.h>
#include <bits/syscall.h>

int pipe(int pipefd[2]) {
	long r;

	r = __haj_syscall1(SYS_pipe, (long)pipefd);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return ((int)r);
}
