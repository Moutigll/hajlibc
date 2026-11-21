/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file toWrite.c
 * @brief Switch a stream to write mode.
 * @Created: 2026/11/21 23:47:26 by Moutig
 * @Updated: 2026/11/21 23:50:49 by Moutig
 *
 * Mirror of toRead.c. If the stream is currently in read mode,
 * drops any unread bytes from the read buffer and repositions the
 * descriptor on the logical position, then clears F_NOWR.
 *
 * Called by fwrite / fputc / fputs / puts / printf just before
 * they touch the write buffer.
 */

#include "stdioImpl.h"
#include <errno.h>
#include <stdio.h>


int	__haj_toWrite(struct FILE *f)
{
	size_t	unread;

	/* Nothing to do if there is no pending read data. */
	if (f->rpos == f->rend)
	{
		f->flags &= ~F_NOWR;
		return (0);
	}
	unread = (size_t)(f->rend - f->rpos);
	/*
	 * Reposition the descriptor back by the number of unread
	 * bytes. If the backend is not seekable (pipe, tty), the
	 * bytes are lost, which POSIX allows: "the file offset is
	 * unspecified" after a read-then-write on an unseekable
	 * update stream.
	 */
	if (f->seek != NULL)
	{
		if (f->seek(f, -(off_t)unread, SEEK_CUR) == (off_t)-1
			&& errno != ESPIPE)
		{
			f->flags |= F_ERR;
			return (-1);
		}
	}
	f->rpos = f->rend;
	f->flags &= ~F_NOWR;
	return (0);
}
