/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file freopen.c
 * @brief Reopen a stream on a new file.
 * @Created: 2026/11/22 03:43:39 by Moutig
 * @Updated: 2026/11/22 22:53:49 by Moutig
 *
 * freopen() closes the descriptor of @p f, opens @p path with the
 * new mode, and reinitialises @p f in place. The FILE object is
 * preserved (same address), which matters for the standard streams
 * and for any user code that kept a pointer to the stream.
 *
 * The buffer is preserved across the call: an existing buffer is
 * reused as is, otherwise one is allocated by __haj_allocBuf. On
 * failure, @p f is left in an
 * undefined state (POSIX allows it), but we do our best to keep
 * it valid: if the open(2) fails, the old descriptor is already
 * closed, and @p f is marked with F_ERR.
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

FILE	*freopen(const char *path, const char *mode, FILE *f)
{
	int		oflags;
	int		fdflags;
	int		fd;

	if (f == NULL || mode == NULL)
	{
		errno = EINVAL;
		return (NULL);
	}
	if (__haj_modeToFlags(mode, &oflags, &fdflags) < 0)
		return (NULL);
	/*
	 * Flush pending writes and close the old fd. We do not call
	 * fclose() because that would free the FILE and remove it
	 * from the global list; we want to keep both.
	 */
	/* Keep going on error: POSIX does not require freopen to
	 * preserve the old stream on failure. */
	if (f->write != NULL)
		(void)__haj_flushWrites(f);
	if (f->close != NULL)
		f->close(f);
	f->wpos = f->wbase;
	f->rpos = f->rend;
	f->flags &= ~(F_EOF | F_ERR | F_NORD | F_NOWR | F_APP | F_LBF | F_UNBUF
		| F_SVB);
	if (path == NULL)
	{
		/*
		 * "If path is NULL, freopen attempts to change the mode
		 * of the stream" (C11). Without /proc/self/fd, we cannot
		 * reopen the same file; fail explicitly.
		 */
		f->flags |= F_ERR;
		errno = EINVAL;
		return (NULL);
	}
	fd = open(path, oflags, 0666);
	if (fd < 0)
	{
		f->flags |= F_ERR;
		return (NULL);
	}
	f->flags |= (unsigned)fdflags;
	/* Reuses the buffer if it exists, otherwise allocates one. */
	__haj_fdInit(f, fd);
	return (f);
}
