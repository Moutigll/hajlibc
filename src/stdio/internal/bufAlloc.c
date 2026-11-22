/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file bufAlloc.c
 * @brief Allocate the read/write buffer of a stream.
 * @Created: 2026/11/22 03:45:47 by Moutig
 * @Updated: 2026/11/22 04:03:47 by Moutig
 *
 * Shared by fopen, fdopen and freopen: picks the buffer size from
 * st_blksize when fstat succeeds, falls back to BUFSIZ, and falls
 * back to a zero-size buffer if malloc fails (the stream then
 * behaves as unbuffered instead of the open failing).
 *
 * The buffer is marked F_MYBUF when it is allocated here, so that
 * fclose knows it must free it. A pre-existing buffer is left
 * untouched: freopen reuses the buffer across calls.
 */

#include "stdioImpl.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

void	__haj_allocBuf(struct FILE *f)
{
	struct stat	st;
	size_t		size;

	if (f->buf != NULL)
		return ;
	size = BUFSIZ;
	if (fstat(f->fd, &st) == 0 && st.st_blksize > 0)
		size = (size_t)st.st_blksize;
	f->buf = malloc(size);
	if (f->buf == NULL)
	{
		f->bufSize = 0;
		return ;
	}
	f->bufSize = size;
	f->flags |= F_MYBUF;
}
