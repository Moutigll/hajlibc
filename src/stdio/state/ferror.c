/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file ferror.c
 * @brief Test the error indicator of a stream.
 * @Created: 2026/11/22 23:01:22 by Moutig
 * @Updated: 2026/11/22 23:01:45 by Moutig
 *
 * ferror() returns non-zero if the error indicator is set on the
 * stream. The indicator is set by any I/O operation that fails
 * (read, write, seek, close) and is cleared by clearerr() or a
 * successful fseek().
 *
 * ferror() does not return the error code: the caller reads errno
 * for that, immediately after the failing operation.
 */

#include "../internal/stdioImpl.h"
#include <stdio.h>

int	ferror(FILE *f)
{
	if (f == NULL)
		return (0);
	return ((f->flags & F_ERR) != 0);
}
