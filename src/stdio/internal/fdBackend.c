/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fdBackend.c
 * @brief read/write/seek/close backends over a file descriptor.
 * @Created: 2026/11/21 23:32:01 by Moutig
 * @Updated: 2026/11/21 23:33:31 by Moutig
 *
 * Implements the four function pointers carried by struct FILE for
 * a stream backed by a file descriptor. These are the backends used
 * by fopen, fdopen and freopen; string and memory streams install
 * their own backends instead.
 *
 * The read and write backends loop on partial results and EINTR so
 * that the stdio layer above (fread / fwrite) can assume a single
 * call returns the full request or a definitive EOF / error. The
 * write backend never loops forever on a zero-byte write: a zero
 * return is treated as an error, because write(2) only returns 0
 * when count is 0, and the stdio layer never calls it with 0.
 *
 * The close backend closes the descriptor and marks the stream as
 * invalid. The seek backend is a straight lseek(2): the stdio layer
 * is responsible for flushing the write buffer and discarding the
 * read buffer before calling it (see toRead.c / toWrite.c).
 */

#include "stdioImpl.h"
#include <errno.h>
#include <unistd.h>

size_t	__haj_fdRead(struct FILE *f, unsigned char *buf, size_t n)
{
	size_t	total;
	ssize_t	r;

	total = 0;
	while (total < n)
	{
		r = read(f->fd, buf + total, n - total);
		if (r < 0)
		{
			if (errno == EINTR)
				continue ;
			f->flags |= F_ERR;
			return ((size_t)-1);
		}
		if (r == 0)
			break ;
		total += (size_t)r;
	}
	return (total);
}

size_t	__haj_fdWrite(struct FILE *f, const unsigned char *buf, size_t n)
{
	size_t	total;
	ssize_t	r;

	total = 0;
	while (total < n)
	{
		r = write(f->fd, buf + total, n - total);
		if (r < 0)
		{
			if (errno == EINTR)
				continue ;
			f->flags |= F_ERR;
			return ((size_t)-1);
		}
		if (r == 0)
		{
			errno = EIO;
			f->flags |= F_ERR;
			return ((size_t)-1);
		}
		total += (size_t)r;
	}
	return (total);
}

off_t	__haj_fdSeek(struct FILE *f, off_t off, int whence)
{
	return (lseek(f->fd, off, whence));
}

int	__haj_fdClose(struct FILE *f)
{
	int	r;

	if (f->fd < 0)
		return (0);
	r = close(f->fd);
	f->fd = -1;
	return (r);
}
