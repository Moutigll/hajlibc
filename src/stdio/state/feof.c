/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file feof.c
 * @brief Test the end-of-file indicator of a stream.
 * @Created: 2026/11/22 23:00:32 by Moutig
 * @Updated: 2026/11/22 23:01:05 by Moutig
 *
 * feof() returns non-zero if the end-of-file indicator is set on
 * the stream. The indicator is set by a read that reaches EOF and
 * is cleared by clearerr(), rewind(), a successful fseek(), or
 * ungetc().
 *
 * feof() does not test whether the next read will hit EOF: it
 * tests whether the *previous* read did. This is the standard
 * "while (!feof(f)) fread(...)" pitfall.
 */

#include "../internal/stdioImpl.h"
#include <stdio.h>

int	feof(FILE *f)
{
	if (f == NULL)
		return (0);
	return ((f->flags & F_EOF) != 0);
}
