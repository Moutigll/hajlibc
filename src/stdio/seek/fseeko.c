/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fseeko.c
 * @brief Reposition a stream (off_t variant).
 * @Created: 2026/11/22 23:16:50 by Moutig
 * @Updated: 2026/11/22 23:18:04 by Moutig
 *
 * Flushes pending writes, drops the read window (__haj_dropRead
 * moves the descriptor back over the unread bytes, so the kernel
 * offset is the logical position and SEEK_CUR needs no correction),
 * then seeks through the backend. On success F_EOF is cleared.
 *
 * A seek that the backend rejects (ESPIPE on a pipe, EINVAL on a
 * bad whence or a negative target) returns -1 without setting the
 * error indicator: no I/O failed. A failed flush does set it.
 *
 * fseek is a wrapper over this function; fseeko takes an off_t, so
 * it handles large files on 32-bit platforms too.
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>
#include <errno.h>

int	fseeko(FILE *f, off_t offset, int whence)
{
	if (f == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	__haj_lockFile(f);
	if (__haj_flushWrites(f) < 0 || __haj_dropRead(f) < 0)
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
	if (f->seek(f, offset, whence) == (off_t)-1)
	{
		__haj_unlockFile(f);
		return (-1);
	}
	f->flags &= ~F_EOF;
	__haj_unlockFile(f);
	return (0);
}
