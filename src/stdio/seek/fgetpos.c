/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fgetpos.c
 * @brief Get the current position of a stream.
 * @Created: 2026/11/22 23:20:39 by Moutig
 * @Updated: 2026/11/22 23:21:16 by Moutig
 *
 * fgetpos() stores the current position of the stream in *pos.
 * The value is opaque: it can only be passed to fsetpos.
 *
 * POSIX says the value is implementation-defined and includes the
 * multibyte conversion state (mbstate_t). For a byte-oriented
 * stream (the only kind we support for now), the position is just
 * the off_t returned by ftello.
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>
#include <errno.h>

int	fgetpos(FILE *f, fpos_t *pos)
{
	off_t	p;

	if (f == NULL || pos == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	p = ftello(f);
	if (p == (off_t)-1)
		return (-1);
	pos->__pos = p;
	return (0);
}
