/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file toRead.c
 * @brief Switch a stream to read mode.
 * @Created: 2026/11/21 23:44:37 by Moutig
 * @Updated: 2026/11/21 23:47:14 by Moutig
 *
 * POSIX requires a stream opened for update ("r+", "w+", "a+") to
 * alternate between read and write, with an fflush or a file
 * positioning call in between. __haj_toRead centralises the
 * switch: if the stream is currently in write mode, it flushes the
 * write buffer and repositions the descriptor on the logical
 * position, then drops any pending read buffer (there is none, by
 * construction). A stream opened write-only (F_NORD) fails with
 * EBADF and F_ERR set.
 *
 * The function is called by fread / fgetc / fgets / getline /
 * scanf just before they touch the read buffer, and by fseek /
 * ftell / fflush when they need a consistent logical position.
 */

#include "stdioImpl.h"
#include <errno.h>
#include <stdio.h>

int	__haj_toRead(struct FILE *f)
{
	size_t	pending;
	size_t	n;

	if (f->flags & F_NORD)
	{
		f->flags |= F_ERR;
		errno = EBADF;
		return (-1);
	}
	/* Nothing to do if there is no pending write data. */
	if (f->wpos == f->wbase)
		return (0);
	/*
	 * Flush the write buffer through the backend.
	 */
	pending = (size_t)(f->wpos - f->wbase);
	n = f->write(f, f->wbase, pending);
	if (n == (size_t)-1)
		return (-1);
	/*
	 * If the backend wrote less than requested, the stream is in
	 * an unrecoverable state: F_ERR is already set, report it.
	 */
	if (n != pending)
	{
		errno = EIO;
		f->flags |= F_ERR;
		return (-1);
	}
	f->wpos = f->wbase;
	/*
	 * Reposition the descriptor on the logical position: the
	 * write side advanced the kernel offset past the bytes we
	 * just flushed, and the read side must start there. If the
	 * backend has no seek (pipe, tty), skip; POSIX says the
	 * stream is unseekable and the caller must not rely on it.
	 */
	if (f->seek != NULL)
	{
		if (f->seek(f, 0, SEEK_CUR) == (off_t)-1)
		{
			/*
			 * ESPIPE means "not seekable": this is fine for
			 * pipes and ttys, the descriptor is already at
			 * the right offset. Any other error is fatal.
			 */
			if (errno != ESPIPE)
			{
				f->flags |= F_ERR;
				return (-1);
			}
		}
	}
	return (0);
}
