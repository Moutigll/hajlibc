/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fopen.c
 * @brief Open a file stream.
 * @Created: 2026/11/22 03:25:07 by Moutig
 * @Updated: 2026/11/22 22:53:41 by Moutig
 *
 * fopen() translates the mode string through __haj_modeToFlags(),
 * opens the descriptor with open(2), allocates the FILE and the
 * buffer, installs the fd backend, and registers the stream in the
 * global open-file list.
 *
 * Buffer allocation is lazy in the sense that the size is picked
 * from st_blksize when fstat succeeds, and from BUFSIZ otherwise.
 * The buffer itself is allocated eagerly (a FILE without a buffer
 * cannot do I/O), but the stream falls back to a zero-size buffer
 * if malloc fails, which turns the stream unbuffered instead of
 * failing the open. This matches what glibc and musl do: fopen
 * only fails on the open(2) error, never on a malloc failure.
 *
 * If the fd is a terminal, the stream is line-buffered (POSIX
 * requires this for stdout, and it is the sane default for any
 * tty). Otherwise it is fully buffered.
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

FILE	*fopen(const char *path, const char *mode)
{
	int		oflags;
	int		fdflags;
	int		fd;
	struct FILE	*f;

	if (path == NULL || mode == NULL)
	{
		errno = EINVAL;
		return (NULL);
	}
	if (__haj_modeToFlags(mode, &oflags, &fdflags) < 0)
		return (NULL);
	fd = open(path, oflags, 0666);
	if (fd < 0)
		return (NULL);
	f = __haj_newFdFile(fd, fdflags);
	if (f == NULL)
		close(fd);
	return (f);
}
