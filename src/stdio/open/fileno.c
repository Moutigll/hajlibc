/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file fileno.c
 * @brief Return the file descriptor of a stream.
 * @Created: 2026/11/22 03:44:05 by Moutig
 * @Updated: 2026/11/22 04:10:02 by Moutig
 *
 * fileno() returns the descriptor carried by the stream. The
 * caller may use it for operations not provided by stdio (fstat,
 * fcntl, ...), but should not close it directly: fclose will do
 * that and a double close would corrupt the descriptor table.
 */

#include "../internal/stdioImpl.h"
#include <stdio.h>
#include <errno.h>

int	fileno(FILE *f)
{
	if (f == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	return (f->fd);
}
