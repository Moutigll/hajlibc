/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fwrite.c
 * @brief Write a block of data to a stream.
 * @Created: 2026/11/22 22:31:27 by Moutig
 * @Updated: 2026/11/22 22:53:28 by Moutig
 *
 * fwrite() copies @p nmemb * @p size bytes from @p ptr to the
 * stream. It returns the number of *elements* actually written
 * (not bytes); a short return means an error, distinguished by
 * ferror().
 *
 * The write path has three branches:
 *   - unbuffered (F_UNBUF): every write goes through the backend;
 *   - large request (>= buffer size): bypass the buffer and write
 *     directly through the backend (avoids a copy);
 *   - small request: copy into the buffer, flush when full or
 *     when a line-buffered stream copies a '\n'.
 *
 * Append mode needs nothing here: the fd backend opens with
 * O_APPEND, so the kernel moves to the end on every write(2).
 * A future memstream backend must handle it in its own write.
 *
 * The write backend writes the full request or fails with F_ERR
 * set (see fdBackend.c), so no partial-write loop is needed here.
 *
 * On error, F_ERR is set. The flag is sticky until clearerr or
 * rewind.
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief Number of elements of @p request bytes really written when
 *        a flush fails with @p done bytes accepted.
 *
 * The bytes still pending in the buffer were not written; some of
 * them may come from an earlier call.
 *
 * @param f    Stream.
 * @param done Bytes of this call copied into the buffer.
 * @param size Element size.
 * @return Number of complete elements written.
 */
static size_t	hajWritten(struct FILE *f, size_t done, size_t size)
{
	size_t	pending;

	pending = (size_t)(f->wpos - f->wbase);
	if (pending >= done)
		return (0);
	return ((done - pending) / size);
}

size_t	fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f)
{
	const unsigned char	*src;
	size_t				request;
	size_t				done;
	size_t				take;
	int					lineHit;
	int					failed;

	if (f == NULL || ptr == NULL || size == 0 || nmemb == 0)
		return (0);
	if (__haj_blockSize(f, size, nmemb, &request) < 0)
		return (0);
	src = (const unsigned char *)ptr;
	__haj_lockFile(f);
	if (__haj_toWrite(f) < 0)
	{
		__haj_unlockFile(f);
		return (0);
	}
	/*
	 * Unbuffered or large request: flush what is pending, then
	 * write directly through the backend.
	 */
	if ((f->flags & F_UNBUF) || request >= f->bufSize)
	{
		if (__haj_flushWrites(f) < 0 || f->write(f, src, request) == (size_t)-1)
		{
			__haj_unlockFile(f);
			return (0);
		}
		__haj_unlockFile(f);
		return (nmemb);
	}
	/*
	 * Small request: copy into the buffer, flush when full or
	 * when a line-buffered stream copies its terminator. Only the
	 * chunk just copied is scanned for the terminator.
	 */
	done = 0;
	failed = 0;
	while (!failed && done < request)
	{
		if (f->wpos == f->wend && __haj_flushWrites(f) < 0)
		{
			failed = 1;
			break ;
		}
		take = (size_t)(f->wend - f->wpos);
		if (take > request - done)
			take = request - done;
		memcpy(f->wpos, src + done, take);
		lineHit = (f->flags & F_LBF)
			&& memchr(f->wpos, f->lbf, take) != NULL;
		f->wpos += take;
		done += take;
		if (lineHit && __haj_flushWrites(f) < 0)
			failed = 1;
	}
	if (failed)
	{
		done = hajWritten(f, done, size);
		__haj_unlockFile(f);
		return (done);
	}
	__haj_unlockFile(f);
	return (nmemb);
}
