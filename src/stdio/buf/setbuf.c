/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file setbuf.c
 * @brief Set the buffer of a stream (legacy interface).
 * @Created: 2026/11/22 20:54:21 by Moutig
 * @Updated: 2026/11/22 20:54:46 by Moutig
 *
 * setbuf(f, buf) is equivalent to:
 *   - setvbuf(f, buf, _IOFBF, BUFSIZ) if buf is non-NULL
 *   - setvbuf(f, NULL, _IONBF, 0)     if buf is NULL
 *
 * POSIX marks setbuf as obsolescent; new code should use setvbuf.
 * The function is provided for compatibility with existing code.
 */

#include "../internal/stdioImpl.h"
#include <stdio.h>

void	setbuf(FILE *f, char *buf)
{
	if (buf != NULL)
		setvbuf(f, buf, _IOFBF, BUFSIZ);
	else
		setvbuf(f, NULL, _IONBF, 0);
}
