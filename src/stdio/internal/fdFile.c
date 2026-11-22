/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fdFile.c
 * @brief Set up a stream over a file descriptor.
 * @Created: 2026/11/22 22:52:12 by Moutig
 * @Updated: 2026/11/22 22:52:49 by Moutig
 *
 * Shared by fopen, fdopen and freopen: install the fd backend,
 * allocate the buffer (see bufAlloc.c), reset the windows and pick
 * the buffering mode (line-buffered on a terminal, fully buffered
 * otherwise).
 */

#include "stdioImpl.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void	__haj_fdInit(struct FILE *f, int fd)
{
	f->fd = fd;
	f->read = __haj_fdRead;
	f->write = __haj_fdWrite;
	f->seek = __haj_fdSeek;
	f->close = __haj_fdClose;
	__haj_allocBuf(f);
	__haj_resetWindows(f);
	if (isatty(fd))
	{
		f->flags |= F_LBF;
		f->lbf = '\n';
	}
	else
		f->lbf = -1;
}

struct FILE	*__haj_newFdFile(int fd, int fdflags)
{
	struct FILE	*f;

	f = malloc(sizeof(*f));
	if (f == NULL)
		return (NULL);
	memset(f, 0, sizeof(*f));
	f->flags = (unsigned)fdflags;
	__haj_fdInit(f, fd);
	__haj_oflAdd(f);
	return (f);
}
