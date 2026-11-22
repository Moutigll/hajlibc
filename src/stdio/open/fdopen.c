/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fdopen.c
 * @brief Associate a stream with an existing file descriptor.
 * @Created: 2026/11/22 03:43:21 by Moutig
 * @Updated: 2026/11/22 22:53:36 by Moutig
 *
 * fdopen() wraps an existing descriptor into a FILE without
 * opening anything. The mode string must be compatible with the
 * descriptor's access mode; fdopen does not verify this (POSIX
 * leaves it undefined, and fstat does not tell the access mode
 * anyway). The caller is responsible for the mode.
 *
 * Like fopen, the stream is fully buffered on a regular file and
 * line-buffered on a terminal. A zero-size buffer is used if
 * malloc fails.
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>
#include <errno.h>

FILE	*fdopen(int fd, const char *mode)
{
	int		oflags;
	int		fdflags;

	if (fd < 0 || mode == NULL)
	{
		errno = EINVAL;
		return (NULL);
	}
	if (__haj_modeToFlags(mode, &oflags, &fdflags) < 0)
		return (NULL);
	return (__haj_newFdFile(fd, fdflags));
}
