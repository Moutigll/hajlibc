/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fclose.c
 * @brief Close a stream.
 * @Created: 2026/11/22 03:43:51 by Moutig
 * @Updated: 2026/11/22 04:07:59 by Moutig
 *
 * fclose() flushes pending writes, closes the descriptor, frees
 * the buffer if it was allocated by stdio, and removes the stream
 * from the global open-file list. For a standard stream (F_PERM),
 * it flushes but does not close the descriptor and does not free
 * the FILE: the pointer remains valid and the stream can be
 * reused by freopen, per POSIX.
 *
 * fclose(NULL) is a no-op that returns 0, as POSIX allows.
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/**
 * @brief Flush pending writes of @p f through its backend.
 *
 * @param f Stream.
 * @return 0 on success, -1 on error (F_ERR set).
 */
static int	hajFlushWrites(struct FILE *f)
{
	size_t	n;

	if (f->wpos == f->wbase)
		return (0);
	n = f->write(f, f->wbase, (size_t)(f->wpos - f->wbase));
	if (n == (size_t)-1)
		return (-1);
	f->wpos = f->wbase;
	return (0);
}

int	fclose(FILE *f)
{
	int	r;

	if (f == NULL)
		return (0);
	/*
	 * Unlink before locking the stream: the list lock is always
	 * taken before a FILE lock (see stdioImpl.h), so removing it
	 * while holding f's lock could deadlock with __haj_oflForeach.
	 * Standard streams are never in the list.
	 */
	if (!(f->flags & F_PERM))
		__haj_oflRemove(f);
	__haj_lockFile(f);
	r = 0;
	if (hajFlushWrites(f) < 0)
		r = EOF;
	if (f->flags & F_PERM)
	{
		/*
		 * Standard stream: flush only, keep the FILE alive so
		 * that freopen can reuse it and so that the public
		 * pointer (stdout, ...) stays valid.
		 */
		__haj_unlockFile(f);
		return (r);
	}
	if (f->close != NULL && f->close(f) < 0)
		r = EOF;
	if ((f->flags & F_MYBUF) && f->buf != NULL)
		free(f->buf);
	f->buf = NULL;
	f->bufSize = 0;
	__haj_unlockFile(f);
	free(f);
	return (r);
}
