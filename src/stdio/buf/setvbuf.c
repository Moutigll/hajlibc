/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file setvbuf.c
 * @brief Set the buffering mode of a stream.
 * @Created: 2026/11/22 20:53:00 by Moutig
 * @Updated: 2026/11/22 21:00:09 by Moutig
 *
 * setvbuf() sets the buffering mode and, optionally, the buffer
 * itself. It must be called after fopen and before any I/O; a
 * second call returns EINVAL (POSIX leaves it undefined; glibc
 * accepts it, hajlibc rejects it). The F_SVB flag records the
 * first call and is cleared by freopen.
 *
 * The buffer can be:
 *   - NULL: stdio allocates one (or uses no buffer for _IONBF)
 *   - provided by the caller: stdio uses it as-is, does not free it;
 *     a size of 0 is rejected with EINVAL
 *
 * The new buffer is allocated before the old one is freed, so a
 * malloc failure leaves the stream untouched.
 *
 * _IONBF turns the stream unbuffered: every fwrite / fread goes
 * through the backend. The buffer (if any) is freed.
 *
 * _IOFBF and _IOLBF set the mode; the buffer is used as given.
 * For _IOLBF, lbf is set to '\n' so that fwrite flushes on
 * newline.
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int	setvbuf(FILE *f, char *buf, int mode, size_t size)
{
	unsigned char	*newBuf;
	size_t			newSize;
	unsigned		newFlags;

	if (f == NULL || (mode != _IOFBF && mode != _IOLBF && mode != _IONBF)
		|| (mode != _IONBF && buf != NULL && size == 0))
	{
		errno = EINVAL;
		return (-1);
	}
	__haj_lockFile(f);
	if (f->flags & F_SVB)
	{
		__haj_unlockFile(f);
		errno = EINVAL;
		return (-1);
	}
	newBuf = NULL;
	newSize = 0;
	newFlags = F_SVB;
	if (mode == _IONBF)
		newFlags |= F_UNBUF;
	else if (buf != NULL)
	{
		newBuf = (unsigned char *)buf;
		newSize = size;
	}
	else
	{
		newSize = (size == 0) ? BUFSIZ : size;
		newBuf = malloc(newSize);
		if (newBuf == NULL)
		{
			__haj_unlockFile(f);
			errno = ENOMEM;
			return (-1);
		}
		newFlags |= F_MYBUF;
	}
	if (mode == _IOLBF)
		newFlags |= F_LBF;
	/* Free the buffer allocated by stdio, if any. */
	if ((f->flags & F_MYBUF) && f->buf != NULL)
		free(f->buf);
	f->flags &= ~(F_LBF | F_UNBUF | F_SVB | F_MYBUF);
	f->flags |= newFlags;
	f->lbf = (mode == _IOLBF) ? '\n' : -1;
	f->buf = newBuf;
	f->bufSize = newSize;
	f->rpos = f->buf;
	f->rend = f->buf;
	f->wbase = f->buf;
	f->wpos = f->buf;
	f->wend = f->buf + f->bufSize;
	__haj_unlockFile(f);
	return (0);
}
