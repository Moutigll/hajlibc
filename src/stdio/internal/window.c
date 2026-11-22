/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file window.c
 * @brief Read / write window helpers shared by the stdio layer.
 * @Created: 2026/11/22 22:52:30 by Moutig
 * @Updated: 2026/11/22 22:53:14 by Moutig
 *
 * The buffer of a stream holds either pending write bytes
 * [wbase, wpos) or unread bytes [rpos, rend), never both. These
 * helpers empty one side or reset both, so that fflush, fclose,
 * freopen, setvbuf, fread, fwrite and the mode switches share a
 * single implementation.
 */

#include "stdioImpl.h"
#include <errno.h>
#include <stdio.h>

int	__haj_flushWrites(struct FILE *f)
{
	if (f->wpos == f->wbase)
		return (0);
	/* The backend writes everything or fails with F_ERR set. */
	if (f->write(f, f->wbase, (size_t)(f->wpos - f->wbase)) == (size_t)-1)
		return (-1);
	f->wpos = f->wbase;
	return (0);
}

int	__haj_dropRead(struct FILE *f)
{
	off_t	unread;

	if (f->rpos == f->rend)
		return (0);
	/*
	 * Move the descriptor back over the bytes read from the kernel
	 * but not consumed. Without seek (or ESPIPE: pipe, tty) they are
	 * lost, which POSIX allows on unseekable streams.
	 */
	if (f->seek != NULL)
	{
		unread = (off_t)(f->rend - f->rpos);
		if (f->seek(f, -unread, SEEK_CUR) == (off_t)-1
			&& errno != ESPIPE)
		{
			f->flags |= F_ERR;
			return (-1);
		}
	}
	f->rpos = f->rend;
	return (0);
}
