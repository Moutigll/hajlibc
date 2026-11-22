/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file ftello.c
 * @brief Return the current position of a stream (off_t variant).
 * @Created: 2026/11/22 23:19:20 by Moutig
 * @Updated: 2026/11/22 23:19:43 by Moutig
 *
 * Returns the logical position of the stream: pending writes are
 * flushed first, then the result is the kernel offset minus the
 * bytes still unread in the read window.
 *
 * On an unseekable stream (pipe, tty) it returns -1 with
 * errno = ESPIPE and leaves the error indicator alone, like musl
 * and glibc. A failed flush does set it.
 *
 * ftell is a wrapper over this function that also checks that the
 * result fits in a long.
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>
#include <errno.h>

off_t	ftello(FILE *f)
{
	off_t	pos;

	if (f == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	__haj_lockFile(f);
	if (__haj_flushWrites(f) < 0)
	{
		__haj_unlockFile(f);
		return (-1);
	}
	if (f->seek == NULL)
	{
		__haj_unlockFile(f);
		errno = ESPIPE;
		return (-1);
	}
	pos = f->seek(f, 0, SEEK_CUR);
	if (pos != (off_t)-1)
		pos -= (off_t)(f->rend - f->rpos);
	__haj_unlockFile(f);
	return (pos);
}
