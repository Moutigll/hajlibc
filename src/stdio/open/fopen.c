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
 * @Updated: 2026/11/22 04:06:44 by Moutig
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
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

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
	f = malloc(sizeof(*f));
	if (f == NULL)
	{
		close(fd);
		return (NULL);
	}
	memset(f, 0, sizeof(*f));
	f->fd = fd;
	f->flags = (unsigned)fdflags;
	f->read = __haj_fdRead;
	f->write = __haj_fdWrite;
	f->seek = __haj_fdSeek;
	f->close = __haj_fdClose;
	__haj_allocBuf(f);
	f->rpos = f->buf;
	f->rend = f->buf;
	f->wbase = f->buf;
	f->wpos = f->buf;
	f->wend = f->buf + f->bufSize;
	if (isatty(fd))
	{
		f->flags |= F_LBF;
		f->lbf = '\n';
	}
	else
		f->lbf = -1;
	__haj_oflAdd(f);
	return (f);
}
