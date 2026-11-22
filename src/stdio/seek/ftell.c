/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file ftell.c
 * @brief Return the current position of a stream.
 * @Created: 2026/11/22 23:18:29 by Moutig
 * @Updated: 2026/11/22 23:18:55 by Moutig
 *
 * Wrapper over ftello (see ftello.c) with the ISO C long return
 * type. A position that does not fit in a long (large file on a
 * 32-bit platform) fails with EOVERFLOW, as POSIX requires.
 */

#include "../internal/stdioImpl.h"

#include <stdio.h>
#include <errno.h>
#include <limits.h>

long	ftell(FILE *f)
{
	off_t	pos;

	pos = ftello(f);
	if (pos > (off_t)LONG_MAX)
	{
		errno = EOVERFLOW;
		return (-1);
	}
	return ((long)pos);
}
