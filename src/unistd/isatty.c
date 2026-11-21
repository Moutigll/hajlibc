/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file isatty.c
 * @brief Test whether a file descriptor refers to a terminal.
 * @Created: 2026/11/22 00:03:46 by Moutig
 * @Updated: 2026/11/22 00:17:44 by Moutig
 *
 * The implementation issues TCGETS on the descriptor. On Linux,
 * this ioctl succeeds on a tty and fails with ENOTTY on anything
 * else (regular file, pipe, socket, ...). errno is set to ENOTTY
 * on failure, as POSIX requires.
 */

#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

int	isatty(int fd)
{
	struct termios	t;

	if (ioctl(fd, TCGETS, &t) == 0)
		return (1);
	errno = ENOTTY;
	return (0);
}
