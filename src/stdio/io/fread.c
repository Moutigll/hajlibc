/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fread.c
 * @brief Read a block of data from a stream.
 * @Created: 2026/11/22 22:27:28 by Moutig
 * @Updated: 2026/11/22 22:30:25 by Moutig
 *
 * fread() copies @p nmemb * @p size bytes from the stream into
 * @p ptr. It returns the number of *elements* actually read (not
 * bytes); a short return means EOF or error, distinguished by
 * feof() / ferror().
 *
 * The bytes already in the read window are always consumed first.
 * Then, until the request is satisfied or EOF / error:
 *   - if what remains is at least as large as the buffer, read
 *     directly into @p ptr through the backend (no extra copy,
 *     as musl does);
 *   - otherwise refill the buffer and copy from it.
 *
 * The backend may return less than requested (tty, pipe), so both
 * branches loop; a zero return is EOF.
 *
 * On EOF, F_EOF is set. On error, F_ERR is set. Both are sticky
 * until clearerr / rewind / fseek / ungetc.
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

/**
 * @brief Copy bytes from the read window of @p f.
 *
 * @param f   Stream.
 * @param dst Destination buffer.
 * @param n   Number of bytes to copy.
 * @return Number of bytes copied.
 */
static size_t	hajReadFromBuf(struct FILE *f, unsigned char *dst, size_t n)
{
	size_t	avail;
	size_t	take;

	avail = (size_t)(f->rend - f->rpos);
	take = (avail < n) ? avail : n;
	if (take > 0)
	{
		memcpy(dst, f->rpos, take);
		f->rpos += take;
	}
	return (take);
}

/**
 * @brief Refill the read buffer of @p f.
 *
 * @param f Stream.
 * @return Number of bytes now buffered, 0 on EOF (F_EOF set), or
 *         (size_t)-1 on error (F_ERR set by the backend).
 */
static size_t	hajRefill(struct FILE *f)
{
	size_t	n;

	n = f->read(f, f->buf, f->bufSize);
	if (n == (size_t)-1)
		return (n);
	f->rpos = f->buf;
	f->rend = f->buf + n;
	if (n == 0)
		f->flags |= F_EOF;
	return (n);
}

size_t	fread(void *ptr, size_t size, size_t nmemb, FILE *f)
{
	unsigned char	*dst;
	size_t			request;
	size_t			done;
	size_t			n;

	if (f == NULL || ptr == NULL || size == 0 || nmemb == 0)
		return (0);
	if (nmemb > SIZE_MAX / size)
	{
		errno = EOVERFLOW;
		f->flags |= F_ERR;
		return (0);
	}
	dst = (unsigned char *)ptr;
	request = size * nmemb;
	__haj_lockFile(f);
	if (__haj_toRead(f) < 0)
	{
		__haj_unlockFile(f);
		return (0);
	}
	done = hajReadFromBuf(f, dst, request);
	while (done < request)
	{
		if (request - done >= f->bufSize)
		{
			n = f->read(f, dst + done, request - done);
			if (n == (size_t)-1)
				break ;
			if (n == 0)
			{
				f->flags |= F_EOF;
				break ;
			}
			done += n;
			continue ;
		}
		n = hajRefill(f);
		if (n == 0 || n == (size_t)-1)
			break ;
		done += hajReadFromBuf(f, dst + done, request - done);
	}
	__haj_unlockFile(f);
	return (done / size);
}
