/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file close.c
 * @brief Implementation of close().
 * @Created: 2026/10/01 08:38:51 by Moutig
 * @Updated: 2026/10/01 08:39:07 by Moutig
 *
 * close() closes a file descriptor. The kernel releases the
 * underlying open file description if this was the last
 * reference. The fd is always freed, even if the close itself
 * fails (POSIX: "the file descriptor shall be deallocated").
 *
 * This is a thin wrapper around the close(2) syscall.
 */

#include <unistd.h>
#include <errno.h>
#include <bits/syscall.h>

int close(int fd)
{
	long r;

	r = __haj_syscall1(SYS_close, fd);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return (0);
}
