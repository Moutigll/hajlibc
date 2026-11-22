/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fflush.c
 * @brief Flush a stream's buffer.
 * @Created: 2026/11/22 20:46:49 by Moutig
 * @Updated: 2026/11/22 22:49:45 by Moutig
 *
 * fflush(f) flushes pending writes through the backend. For a
 * seekable input stream, it also repositions the descriptor on
 * the logical position (POSIX: "the file offset is set to the
 * current position of the stream"), so that a subsequent read(2)
 * by the caller sees the bytes the stream has not consumed.
 *
 * fflush(NULL) flushes stdin / stdout / stderr (never in the list,
 * see fclose.c), then walks the global open-file list and flushes
 * every stream. The list walk uses a trylock on each per-FILE lock
 * so that a thread that already owns a stream does not deadlock
 * against itself (POSIX does not require fflush(NULL) to block).
 * It returns EOF if any stream failed.
 *
 * fflush on an output stream with no pending bytes is a no-op.
 * fflush on an input stream with no read buffer is a no-op.
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>

/**
 * @brief __haj_oflForeach callback for fflush(NULL).
 *
 * @param f   Stream (locked by __haj_oflForeach).
 * @param arg int * error accumulator, set to 1 on failure.
 */
static void	hajFlushCb(struct FILE *f, void *arg)
{
	if (__haj_syncFile(f) < 0)
		*(int *)arg = 1;
}

/**
 * @brief Flush one stream under its lock.
 *
 * @param f Stream.
 * @return 0 on success, -1 on error.
 */
static int	hajFlushOne(struct FILE *f)
{
	int	r;

	__haj_lockFile(f);
	r = __haj_syncFile(f);
	__haj_unlockFile(f);
	return (r);
}

int	fflush(FILE *f)
{
	int	err;

	if (f != NULL)
		return (hajFlushOne(f) < 0 ? EOF : 0);
	err = 0;
	if (hajFlushOne(stdin) < 0)
		err = 1;
	if (hajFlushOne(stdout) < 0)
		err = 1;
	if (hajFlushOne(stderr) < 0)
		err = 1;
	__haj_oflForeach(hajFlushCb, &err, 1);
	return (err ? EOF : 0);
}
